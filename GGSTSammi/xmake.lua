local projectName = "GGSTSammi"

package("cpr")
    add_deps("cmake")
    set_sourcedir(path.join(os.scriptdir(), "cpr"))
    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "ON"))
        import("package.tools.cmake").install(package, configs)
    end)
package_end()

add_requires("cpr")

target(projectName)
    add_packages("cpr")

    add_rules("ue4ss.mod")

    add_includedirs("include", { public = true })
    add_headerfiles("include/**.hpp")
    add_headerfiles("include/**.h")

    add_files("src/dllmain.cpp")
    add_files("src/safetyhook.cpp")

    add_links("crypt32")
