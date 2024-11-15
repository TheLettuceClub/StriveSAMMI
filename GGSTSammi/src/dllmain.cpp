#define NOMINMAX

#include <cpr/cpr.h>
#include <Mod/CppUserModBase.hpp>
#include <SigScanner/SinglePassSigScanner.hpp>
// #include "struct_util.h"
#include <UnrealDef.hpp>
#include <UObjectGlobals.hpp>
#include "safetyhook.hpp"
#include <AGameModeBase.hpp>
#include <LuaMadeSimple/LuaMadeSimple.hpp>
#include "json.hpp"
#include "manual_retype.h"
// #define WIN32_LEAN_AND_MEAN
// #include "Windows.h"
// Mod

using json = nlohmann::json;

// std::wstring convertToWide2(const std::string_view &str)
// {
//     if (str.empty())
//     {
//         return std::wstring();
//     }
//     int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
//     std::wstring wstrTo(size_needed, 0);
//     MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
//     return wstrTo;
// }

NLOHMANN_JSON_SERIALIZE_ENUM(ObjectID, {
                                           {OBJ_Player1, "Player1"},
                                           {OBJ_Player2, "Player2"},
                                           {OBJ_Projectile, "Projectile"},
                                       })

NLOHMANN_JSON_SERIALIZE_ENUM(HitType, {
                                          {HitType::HIT_Normal, "Normal"},
                                          {HitType::HIT_SmallCounter, "SmallCounter"},
                                          {HitType::HIT_MediumCounter, "MediumCounter"},
                                          {HitType::HIT_LargeCounter, "LargeCounter"},
                                          {HitType::HIT_RISCCounter, "RISCCounter"},
                                      })

NLOHMANN_JSON_SERIALIZE_ENUM(RoundEndInfo::WinType, {
                                                        {RoundEndInfo::WIN_Player1, "Player1"},
                                                        {RoundEndInfo::WIN_Player2, "Player2"},
                                                        {RoundEndInfo::WIN_Draw, "Draw"},
                                                    })

NLOHMANN_JSON_SERIALIZE_ENUM(RoundEndInfo::RoundEndCause, {
                                                              {RoundEndInfo::RE_KO, "KO"},
                                                              {RoundEndInfo::RE_TimeOut, "TimeOut"},
                                                          })

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SammiState::PlayerState, charaName, health, tension, dangerBalance, burst, risc, action)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SammiState, roundTimeLimit, roundTimeLeft, p1, p2)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(HitInfo, type, attackLevel, damage, comboCount, comboDamage, attacker, defender, attackerAction, defenderPrevAction)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectCreatedInfo, objName, parent, p1Action, p2Action)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RoundEndInfo, type, cause)

SammiState updateMessage;

class SammiConfig
{
    SammiConfig() = default;

public:
    bool bEnabled{};
    std::string webhookUrl = "http://127.0.0.1:9450/webhook";
    int stateUpdateMs = 1000 / 25;
    int timeoutMs = 0;

    static SammiConfig *GetInstance()
    {
        static SammiConfig instance;
        return &instance;
    }
};

auto sendEvent(std::string eventName, std::string customData, int timeout)
{
    auto body = std::format("{{'trigger': '{}', 'eventInfo': {}}}", eventName, customData);
    cpr::Response r = cpr::Post(cpr::Url{SammiConfig::GetInstance()->webhookUrl},
                                cpr::Body{body},
                                cpr::Timeout{std::chrono::milliseconds(timeout)});
};

bool bIsBattle = false;

void MessageHandler()
{
    auto config = SammiConfig::GetInstance();

    while (true)
    {
        if (bIsBattle)
        {
            json j = updateMessage;
            sendEvent("ggst_stateUpdate", j.dump(), abs(config->timeoutMs));
            // Output::send<LogLevel::Verbose>(STR("StriveSAMMI: sent stateUpdate\n"));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(config->stateUpdateMs));
    }
}

SafetyHookInline UpdateBattleSub_Detour{};

SammiState prevState{};
bool bRoundOver{};
AREDGameState_Battle *saveState;

void UpdateBattleSub_Hook(AREDGameState *pThis, float DeltaSeconds, bool bUpdateDraw)
{
    // Output::send<LogLevel::Verbose>(STR("StriveSAMMI: update_battle_sub happening\n"));
    UpdateBattleSub_Detour.call(pThis, DeltaSeconds, bUpdateDraw);
    saveState = pThis->battleState;

    if (!bUpdateDraw)
    {
        return;
    }

    auto newState = SammiState();

    auto p1 = pThis->battleState->player1Data;
    auto p2 = pThis->battleState->player2Data;

    newState.p1.charaName = (new std::string(p1->getSprite()))->substr(0, 3);
    newState.p2.charaName = (new std::string(p2->getSprite()))->substr(0, 3);

    newState.roundTimeLimit = 2; // dummy values until i find real ones
    newState.roundTimeLeft = 1;

    newState.p1.health = p1->health;
    newState.p2.health = p2->health;

    newState.p1.tension = pThis->battleState->p1Tension;
    newState.p2.tension = pThis->battleState->p2Tension;

    newState.p1.dangerBalance = pThis->battleState->p1Danger; // don't have "tension balance", do have danger values
    newState.p2.dangerBalance = pThis->battleState->p2Danger;

    newState.p1.burst = pThis->battleState->p1Burst;
    newState.p2.burst = pThis->battleState->p2Burst;

    newState.p1.risc = p1->risc;
    newState.p2.risc = p2->risc;

    newState.p1.action = p1->getMoveID();
    newState.p2.action = p2->getMoveID();

    updateMessage = newState;

    if (!bRoundOver && (newState.roundTimeLeft == 0 || newState.p1.health <= 0 || newState.p2.health <= 0))
    {
        bRoundOver = true;

        RoundEndInfo roundEnd;

        if (newState.p2.health > newState.p1.health)
            if (newState.p1.health > newState.p2.health)
                roundEnd.type = RoundEndInfo::WIN_Player1;
            else
                roundEnd.type = RoundEndInfo::WIN_Player2;
        else if (newState.p1.health > newState.p2.health)
            roundEnd.type = RoundEndInfo::WIN_Player1;
        else
            roundEnd.type = RoundEndInfo::WIN_Draw;

        roundEnd.cause = newState.roundTimeLeft == 0 ? RoundEndInfo::RE_TimeOut : RoundEndInfo::RE_KO;

        json j = roundEnd;
        std::thread(sendEvent, "ggst_roundEndEvent", j.dump(), abs(SammiConfig::GetInstance()->timeoutMs)).detach();
        // Output::send<LogLevel::Verbose>(STR("StriveSAMMI: sent roundEndEvent\n"));
    }

    prevState = newState;
}

SafetyHookInline RoundInit_Detour{};

void RoundInit_Hook(void *pThis, bool use2ndInitialize)
{
    // Output::send<LogLevel::Verbose>(STR("StriveSAMMI: roundinit happening\n"));
    RoundInit_Detour.call(pThis, use2ndInitialize);

    bRoundOver = false;

    std::thread(sendEvent, "ggst_roundStartEvent", "{}", abs(SammiConfig::GetInstance()->timeoutMs)).detach();
    // Output::send<LogLevel::Verbose>(STR("StriveSAMMI: sent roundStartEvent\n"));
}

// SafetyHookInline CreateObjectArg_Detour{};

// std::thread objCreateThread;

// void CreateObjectArg_Hook(OBJ_CBase *pThis, const CXXBYTE<32> &actName, int exPoint)
// {
//     Output::send<LogLevel::Verbose>(STR("StriveSAMMI: CreateObjectArg happening\n"));
//     CreateObjectArg_Detour.call(pThis, actName, exPoint);

//     auto p1 = GameState->BattleObjectManager->m_TeamManager[0].m_pMainPlayerObject;
//     auto p2 = GameState->BattleObjectManager->m_TeamManager[1].m_pMainPlayerObject;

//     auto objCreate = ObjectCreatedInfo();

//     objCreate.objName = actName.m_Buf;
//     objCreate.parent = pThis == p1 ? OBJ_Player1 : pThis == p2 ? OBJ_Player2
//                                                                : OBJ_Projectile;
//     objCreate.p1Action = p1->m_CurActionName.m_Buf;
//     objCreate.p2Action = p2->m_CurActionName.m_Buf;

//     json j = objCreate;
//     std::thread(sendEvent, "ggst_objectCreatedEvent", j.dump(), abs(SammiConfig::GetInstance()->timeoutMs)).detach();
//     Output::send<LogLevel::Verbose>(STR("StriveSAMMI: sent objectCreated\n"));
// }

SafetyHookInline ExecuteAttackHit_Detour;

// TODO: projectiles!!!!!
Unreal::uint8 ExecuteAttackHit_Hook(void *pThis, void *dmObj, bool isSpGuardCollision, bool isTransfer)
{
    // Output::send<LogLevel::Verbose>(STR("StriveSAMMI: executeattck_hit happening\n"));
    const auto retVal = ExecuteAttackHit_Detour.call<Unreal::uint8>(pThis, dmObj, isSpGuardCollision, isTransfer);
    auto newHit = HitInfo();
    auto p1 = saveState->player1Data;
    auto p2 = saveState->player2Data;
    // determine who is doing the hitting
    if (p1->atkDmg && p2->recvAtkDmg)
    {
        // if p1's atkdmg is nonzero, then they're attacking, else p2
        newHit.attacker = OBJ_Player1;
        newHit.attackerAction = p1->getMoveID();
        newHit.attackLevel = p1->atkLvl;
        newHit.comboCount = p2->comboCount; // combo count is stored on the player getting comboed
        newHit.comboDamage = p2->totalComboDmg;
        newHit.damage = p2->lastHitRecvDmg; // to get the scaled damage
        // newHit.damage = p1->atkDmg; //to get the unscaled damage
        newHit.defender = OBJ_Player2;
        newHit.defenderPrevAction = p2->getPrevMoveID();
        if (p2->counterHit) // NOTE: this only works with REAL counterhits, simply turning them on in training mode won't suffice, sorry
        {
            newHit.type = p1->getCHType();
        }
        else
        {
            newHit.type = HitType::HIT_Normal;
        }
    }
    else if (p2->atkDmg && p1->recvAtkDmg) // p2 attacking, p1 got hit
    {
        newHit.attacker = OBJ_Player2;
        newHit.attackerAction = p2->getMoveID();
        newHit.attackLevel = p2->atkLvl;
        newHit.comboCount = p1->comboCount; // combo count is stored on the player getting comboed
        newHit.comboDamage = p1->totalComboDmg;
        newHit.damage = p1->lastHitRecvDmg; // to get the scaled damage
        // newHit.damage = p2->atkDmg; //to get the unscaled damage
        newHit.defender = OBJ_Player2;
        newHit.defenderPrevAction = p1->getPrevMoveID();
        if (p1->counterHit)
        {
            newHit.type = p2->getCHType();
        }
        else
        {
            newHit.type = HitType::HIT_Normal;
        }
    }
    else
    {
        return retVal; // else someone did an attack that didn't connect
    }

    json j = newHit;
    std::thread(sendEvent, "ggst_hitEvent", j.dump(), abs(SammiConfig::GetInstance()->timeoutMs)).detach();
    // Output::send<LogLevel::Verbose>(STR("StriveSAMMI: sent hitEvent\n"));
    return retVal;
}

const HitType PlayerData::getCHType() const
{
    // TODO: RISC support
    switch (chTypeFlag[0])
    {
    case (0x1C):
        return HitType::HIT_SmallCounter;

    case (0xFC): // p1 value seemingly
    case (0xCC): // p1 value ig
    case (0xBC): // 5d both sides
        return HitType::HIT_MediumCounter;

    case (0xA4): // p1
    case (0x5C): // p2
        return HitType::HIT_LargeCounter;

    default: // if the values change or i'm wrong, lie
        return HitType::HIT_Normal;
    }
}

class GGSTSammi : public CppUserModBase
{
public:
    std::thread messageHandler;

    GGSTSammi()
    {
        ModName = STR("GGSTSammi");
        ModVersion = STR("1.0");
        ModDescription = STR("Sammi integration for GGST");
        ModAuthors = STR("WistfulHopes");
        // Do not change this unless you want to target a UE4SS version
        // other than the one you're currently building with somehow.
        // ModIntendedSDKVersion = STR("2.6");
        // Output::send<LogLevel::Verbose>(STR("StriveSAMMI Started\n"));
    }

    ~GGSTSammi() override = default;

    auto on_unreal_init() -> void override
    {
        // Output::send<LogLevel::Verbose>(STR("StriveSAMMI: Unreal Started\n"));
        messageHandler = std::thread(MessageHandler);

        Unreal::Hook::RegisterInitGameStatePreCallback([&](Unreal::AGameModeBase *Context)
                                                       {
            bIsBattle = false;
            const auto Class = Unreal::UObjectGlobals::StaticFindObject<UClass*>(
                nullptr, nullptr, to_wstring("/Script/RED.REDGameMode_Battle"));

            if (Context->IsA(Class))
            {
                bIsBattle = true;
				// Output::send<LogLevel::Verbose>(STR("StriveSAMMI: found the gamestate_battle\n"));
            } });

        const SignatureContainer update_battle_sub{
            {{"48 8B C4 48 89 58 ? 41 56 48 81 EC ? ? ? ? 48 89 68"}},
            [&](const SignatureContainer &self)
            {
                UpdateBattleSub_Detour = safetyhook::create_inline(self.get_match_address(), UpdateBattleSub_Hook);
                // Output::send<LogLevel::Verbose>(STR("StriveSAMMI: update_battle_sub happy\n"));
                return true;
            },
            [](SignatureContainer &self) {
            },
        };

        const SignatureContainer round_initialize{
            {{"48 8B C4 48 89 58 ? 48 89 70 ? 48 89 78 ? 55 41 54 41 55 41 56 41 57 48 8D 68 ? 48 81 EC ? ? ? ? 0F 29 70 ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 ? 45 33 ED"}},
            [&](const SignatureContainer &self)
            {
                RoundInit_Detour = safetyhook::create_inline(self.get_match_address(), RoundInit_Hook);
                // Output::send<LogLevel::Verbose>(STR("StriveSAMMI: round_init happy\n"));
                return true;
            },
            [](SignatureContainer &self) {
            },
        };

        // const SignatureContainer create_object_arg{
        //     {{"48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 ? 80 3A"}},
        //     [&](const SignatureContainer &self)
        //     {
        //         CreateObjectArg_Detour = safetyhook::create_inline(self.get_match_address(), CreateObjectArg_Hook);
        //         Output::send<LogLevel::Verbose>(STR("StriveSAMMI: create_object_arg happy\n"));
        //         return true;
        //     },
        //     [](SignatureContainer &self) {
        //     },
        // };

        const SignatureContainer execute_attack_hit{
            {{"48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 8B 82"}},
            [&](const SignatureContainer &self)
            {
                ExecuteAttackHit_Detour = safetyhook::create_inline(self.get_match_address(), ExecuteAttackHit_Hook);
                Output::send<LogLevel::Verbose>(STR("StriveSAMMI: found the execute_attack_hit happy\n"));
                return true;
            },
            [](SignatureContainer &self) {
            },
        };

        std::vector<SignatureContainer> signature_containers;
        signature_containers.push_back(update_battle_sub);
        signature_containers.push_back(round_initialize);
        // signature_containers.push_back(create_object_arg);
        signature_containers.push_back(execute_attack_hit);

        SinglePassScanner::SignatureContainerMap signature_containers_map;
        signature_containers_map.emplace(ScanTarget::MainExe, signature_containers);

        SinglePassScanner::start_scan(signature_containers_map);
    }
};

#define GGST_SAMMI_API __declspec(dllexport)

extern "C"
{
    GGST_SAMMI_API RC::CppUserModBase *start_mod()
    {
        return new GGSTSammi();
    }

    GGST_SAMMI_API void uninstall_mod(RC::CppUserModBase *mod)
    {
        delete mod;
    }
}
