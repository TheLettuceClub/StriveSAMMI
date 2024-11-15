#include "struct_util.h"

enum HitType
{
    HIT_Normal,
    HIT_SmallCounter,
    HIT_MediumCounter,
    HIT_LargeCounter,
    HIT_RISCCounter,
};

class PlayerData
{
public:
    FIELD(0x1200, long, health); // includes scaling and guts and such
    FIELD(0xC99C, long, risc);
    FIELD(0xD618, long, wallDamage);
    ARRAY_FIELD(0x37F8, char[32], moveID);
    ARRAY_FIELD(0x37D8, char[32], prevMoveID);
    ARRAY_FIELD(0x12A0, char[32], spriteName);
    FIELD(0x391, bool, counterHit);      // set if in counterhit *state*, i.e. during the recovery of a move
    FIELD(0xF638, long, afro);           // duration?
    FIELD(0xC344, long, lastHitRecvDmg); // will be scaled
    FIELD(0xC334, long, totalComboDmg);  // will also be scaled
    FIELD(0xc324, long, comboCount);     // set when getting comboed, not when comboing other player
    FIELD(0x750, long, atkType);         // enum this later
    FIELD(0x754, long, atkLvl);
    FIELD(0x75C, long, atkDmg);      // never scaled, always base damage of connected attack from BBS
    FIELD(0xCF0, long, recvAtkType); // these are when getting hit, will always be equivalent to other players non-recv fields
    FIELD(0xCF4, long, recvAtkLvl);
    FIELD(0xCFC, long, recvAtkDmg);
    ARRAY_FIELD(0x3618, byte[2], chTypeFlag); // counter hit type for all attacks. only changed when another attack happens. very small CHes don't change the value, need to deal with them later
    FIELD(0x1C4, long, stateDuration);
    FIELD(0x280, long, hitstop); // hitstop experienced??
    FIELD(0x6240, long, blockstun);
    FIELD(0x9A08, long, hitstun);
    FIELD(0x28C, long, clashStop);
    FIELD(0x394, short, invuln);         // conglomerate of strike/throw/absolute invuln bits, also slowdown activation, todo: decode!!
    FIELD(0x11CC, long, backdashInvuln); // duration?
    FIELD(0xC3D8, long, throwProtect);   // what does this measure??
    FIELD(0xC404, long, crossupProtect);
    FIELD(0x6248, long, remAirJumps); // rem = remaining
    FIELD(0x624C, long, remAirDash);
    FIELD(0xC928, long, bdCooldown);
    FIELD(0x9B0C, long, landRecovery);
    FIELD(0xC3FC, long, ibLockout); // ib = instant block
    FIELD(0xC51F, bool, ibState);
    FIELD(0xC9A0, long, RISCpause);
    FIELD(0xC9A4, long, maxRISCpause);
    FIELD(0xD13C, long, slowdownTimer);
    FIELD(0xC8D0, long, defBoost); // timer or amount??
    FIELD(0xC8CC, long, atkBoost);
    FIELD(0xC8C4, long, PBatkBoost);
    FIELD(0xC8C8, long, PBdefBoost);
    FIELD(0x3A8, long, posX); // not exactly standard coords btw
    FIELD(0x3AC, long, posY);
    FIELD(0x4C8, long, velX);
    FIELD(0x4CC, long, velY);
    FIELD(0x4D0, long, gravity);
    FIELD(0x4EC, long, pushback);
    FIELD(0x4F0, long, pushbackFD); // fd = faultless defense
    FIELD(0xB09, short, guardType);
    FIELD(0x622C, short, cancels); // conglomerate of bitfields for various cancels. todo: decode
    FIELD(0xB98, long, kdType);
    FIELD(0xC54, long, chKDType);
    FIELD(0x83C, long, atkWallDmg);
    FIELD(0x9A80, long, defenseVal); // defense modifier
    FIELD(0x9A84, long, guts);
    FIELD(0x18, long, isActive);

    // projectile only since this structure is used for all entities
    FIELD(0x3B88, short, numHits);
    FIELD(0xB14, short, clashLvl);
    FIELD(0x3D90, short, owner); // which player owns it

    // array getters
    const char *getMoveID() const { return &moveID[0]; }
    const char *getPrevMoveID() const { return &prevMoveID[0]; }
    const char *getSprite() const { return &spriteName[0]; }
    const HitType getCHType() const;
};

class AREDGameState_Battle
{
public:
    FIELD(0x48, long, p1Tension);
    FIELD(0x140, long, p1TensionPenalty);
    FIELD(0x158, long, p1PositiveBonus);
    FIELD(0x144, long, p1TensionPulse);
    FIELD(0x150, long, p1Danger);
    FIELD(0x1458, long, p1Burst);
    FIELD(0x1A8, long, p2Tension);
    FIELD(0x2A0, long, p2TensionPenalty);
    FIELD(0x2A4, long, p2PositiveBonus);
    FIELD(0x2B8, long, p2TensionPulse);
    FIELD(0x2B0, long, p2Danger);
    FIELD(0x145C, long, p2Burst);
    // FIELD(0x36F4, long, camMotionBlur);
    // FIELD(0x1370, long, playerStop); //duration??
    // FIELD(0x1374, long, globalStop);
    FIELD(0x8B8, class PlayerData *, player1Data);
    FIELD(0x8C0, class PlayerData *, player2Data);
};

class AREDGameState // technically these names are wrong but idc
{
public:
    FIELD(0xBB8, class AREDGameState_Battle *, battleState);
};

enum HIT_TYPE
{
    HITTYPE_MISS = 0x0000,
    HITTYPE_DAMAGE = 0x0001,
    HITTYPE_GUARD = 0x0002,
    HITTYPE_DODGE = 0x0003,
    HITTYPE_ARMOR = 0x0004,
    HITTYPE_BOSSARMOR = 0x0005,
    HITTYPE_PARRY = 0x0006,
};

enum ObjectID
{
    OBJ_Player1,
    OBJ_Player2,
    OBJ_Projectile,
};

struct SammiState
{
    struct PlayerState
    {
        std::string charaName{};
        Unreal::int32 health{};
        Unreal::int32 tension{};
        Unreal::int32 dangerBalance{};
        Unreal::int32 burst{};
        Unreal::int32 risc{};
        std::string action{};
    };

    Unreal::int32 roundTimeLimit{};
    Unreal::int32 roundTimeLeft{};
    PlayerState p1{};
    PlayerState p2{};
};

struct HitInfo
{

    HitType type{};
    Unreal::int32 attackLevel{};
    Unreal::int32 damage{};
    Unreal::int32 comboCount{};
    Unreal::int32 comboDamage{};
    ObjectID attacker{};
    ObjectID defender{};
    std::string attackerAction{};
    std::string defenderPrevAction{};
};

struct ObjectCreatedInfo
{
    std::string objName{};
    ObjectID parent{};
    std::string p1Action{};
    std::string p2Action{};
};

struct RoundEndInfo
{
    enum WinType
    {
        WIN_Player1,
        WIN_Player2,
        WIN_Draw,
    };

    enum RoundEndCause
    {
        RE_KO,
        RE_TimeOut,
    };

    WinType type{};
    RoundEndCause cause{};
};