cmake_minimum_required(VERSION 3.16)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the source code and call cmake from there")
endif()

project (gltest CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)


#This is a hack, uses a weird regex to copy libraries into the /bin dir.
#this is a command that shows the rpath: readelf -d exe |head -30
option(PORTABLE "Helps bundle DLL's when you install, also sets rpath on linux" OFF)

#usually you don't really have control over static linking, SDL is an exception.
option(STATIC_BUILD "prefer static libraries" OFF)


#for clang-cl
#clang-cl.exe acts exactly like MSVC (drag-n-drop replacement), 
#but you can use clang++.exe which can generate exactly the same binary,
#but with different command line syntax (and MSVC=FALSE and GNUC=TRUE).
#so MY_CLANGCL means both MSVC and the GNUC.
if("x${CMAKE_CXX_COMPILER_ID}" MATCHES "x.*Clang")
    if("x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
        #called MY_ because I don't want it to clash with anything
        set(MY_CLANGCL TRUE)
    endif()
endif()


#
# global modifications, I wish there was a better way but there isn't, must be done before add_executable
#

#TODO: I use RelWithDebInfo as a "production" build, but maybe I should double check the flags, 
# (I should do this right now but I don't have msvc on me)
# because maybe I don't want /Ob1 and instead should use /Ob2 and make sure /zO is enabled.
# and /INCREMENTAL should not be used for production (maybe use PORTABLE or a new option?)
if(MSVC)
    string( REPLACE "/NDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    
    #silence the warning, note I don't think clang-cl supports incremental linking yet.
    if(USE_ASAN)
        string( REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
    endif()

    #remove the flag that disables /GS on release builds.
    option(USE_MSVC_GS "remove /GS- on msvc" OFF)
    if(USE_MSVC_GS)
        string( REPLACE "/GS-" "/GS" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        string( REPLACE "/GS-" "/GS" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    endif()
else()
    string( REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

# note that I could avoid making this global by using set_property
# but it will not propogate as an interface (I would need to set it for every potential add_library)
if(MSVC OR MY_CLANGCL)
    #note that it is impossible to create a static debug runtime (I would never need it)
    option(FORCE_STATIC_VCRT "forces /MT" OFF)

    #ATM clang-cl doesn't support /MDd so you need this if you want to use clang-cl.
    option(FORCE_NON_DEBUG_VCRT "forces /MD" OFF)
    if(FORCE_STATIC_VCRT)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    elseif(FORCE_NON_DEBUG_VCRT)
        #set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    endif()
endif()

option(USE_LTO "Use link time optimizations" OFF)
if(USE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

#If you install anywhere other than /usr or the default /usr/local
#the libraries will not be accessible without setting rpath (or manually doing LD_LIBRARY_PATH)
if(PORTABLE OR WIN32)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    option(USE_CCACHE "supposed to create speed?" ON)
    if(USE_CCACHE)
        message(STATUS "ccache found")
        set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
    endif()
endif()

#
# flags start
#

option(NO_RTTI "disable run time type information" OFF)
option(NO_EXCEPTIONS "disable exception unwinding" OFF)

#put all the flags here.
set(MY_COMPILER_FLAGS "")

if (MSVC)
    set(MY_COMPILER_FLAGS ${MY_COMPILER_FLAGS} /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE /D_SCL_SECURE_NO_WARNINGS /D_NOMINMAX)
    set(MY_COMPILER_FLAGS ${MY_COMPILER_FLAGS} /wd4244) # C4244: 'conversion_type': conversion from 'type1' to 'type2', possible loss of data
    set(MY_COMPILER_FLAGS ${MY_COMPILER_FLAGS} /wd4068) # C4068: unknown pragma

    #frame pointer ommision ruins stack traces
    if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        #TODO: untested, maybe I need to replace "/Oy"
        set(MY_COMPILER_FLAGS ${MY_COMPILER_FLAGS} /Oy-)
    endif()

    #maybe /analyze?
else()
    
    if(NO_RTTI)
        set(MY_COMPILER_FLAGS ${MY_COMPILER_FLAGS} -fno-rtti)
    endif()
    if(NO_EXCEPTIONS)
        set(MY_COMPILER_FLAGS ${MY_COMPILER_FLAGS} -fno-exceptions)
    endif()

    #this is a big chunk taken from openrtc2, I hope they don't mind, but I like warnings.
    include(CheckCXXCompilerFlag)

    # Check if a flag exists and add it to the list of compiler options
    function (ADD_CHECK_CXX_COMPILER_FLAG _CXXFLAGS _CACHE_VAR _FLAG)
        CHECK_CXX_COMPILER_FLAG("${_FLAG}" "${_CACHE_VAR}")
        if (${_CACHE_VAR})
            set(${_CXXFLAGS} ${${_CXXFLAGS}} ${_FLAG} PARENT_SCOPE)
        else ()
            message(STATUS "Unsupported CXXFLAG: ${_FLAG}")
        endif ()
    endfunction ()

    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_SUGGEST_OVERRIDE -Wsuggest-override) #not in gcc
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_DUPLICATED_COND -Wduplicated-cond) #not in clang
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_NON_VIRTUAL_DTOR -Wnon-virtual-dtor) #not in gcc
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_MISSING_VARIABLE_DECLARATIONS -Wmissing-variable-declarations) #not in gcc
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_DUPLICATED_BRANCHES -Wduplicated-branches) #not in clang
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_RESTRICT -Wrestrict) #part of -Wall in gcc, not in clang.
    #ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_MISSING_FIELD_INITIALIZERS -Wmissing-field-initializers) #enabled by -Wextra on both gcc and clang????
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_UNREACHABLE_CODE_BREAK -Wunreachable-code-aggressive) #not in gcc
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_LOGICAL_OP -Wlogical-op) #not in clang
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_RANGE_LOOP_ANALYSIS -Wloop-analysis) #not in gcc
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_TAUTOLOGICAL_ZERO_COMPARE -Wtautological-constant-in-range-compare) #not in gcc
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_IMPLICIT_FALLTHROUGH -Wimplicit-fallthrough) #not in clang


    # Do not enable for MinGW, as its headers contain redundant declarations of builtin functions (from orct2)
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_REDUNDANT_DECLS -Wredundant-decls) #doesn't do anything on clang

    #these 2 warnings can be tested once in a while, but they can be annoying since final is not neccessary everywhere.
    #ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_SUGGEST_FINAL_TYPES -Wsuggest-final-types)
    #ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_SUGGEST_FINAL_METHODS -Wsuggest-final-methods)


    #flags that I found from -Wconversion but -Wconversion is too overzealous
    #and ruins undefined behavior sanitizer analysis because static_cast's will skip checks.
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_IMPLICIT_FLOAT_CONVERSION -Wimplicit-float-conversion)
    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_IMPLICIT_INT_FLOAT_CONVERSION -Wimplicit-int-float-conversion)

    

    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_SHADOW_ALL -Wshadow-all)

    if(NOT CXX_WARN_SHADOW_ALL)
        set(MY_COMPILER_FLAGS ${MY_COMPILER_FLAGS} -Wshadow)
    endif()

    ADD_CHECK_CXX_COMPILER_FLAG(MY_COMPILER_FLAGS CXX_WARN_ENUM_CONVERSION -Wenum-conversion)
    

    set(MY_COMPILER_FLAGS ${MY_COMPILER_FLAGS} -Wall -Wextra -Wpedantic)
    set(MY_COMPILER_FLAGS ${MY_COMPILER_FLAGS} -Wunused -Wundef -Wmissing-declarations -Wvla)

    #frame pointer ommision ruins stack traces
    if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(MY_COMPILER_FLAGS ${MY_COMPILER_FLAGS} -fno-omit-frame-pointer)
    endif()
endif()

#
# create ALL_SANITIZERS because you can reuse the settings on potential built-in libraries
#

add_library(ALL_SANITIZERS INTERFACE)

#note that you cannot mix sanitizers with eachother.
#asan + ubsan + lsan should be a supported combination

#Also I think that for distribution I could use UBsan + CFI (requires LTO) + safe-stack for low overhead checks.


option(USE_ASAN "Use address sanitizer" OFF)
if(USE_ASAN)
    
    if(MSVC)
        #make sure to globally set this if you are running without a debugger:
        #set ASAN_VCASAN_DEBUGGING=1
        #alternatively if you can use a core dump instead:
        #set ASAN_SAVE_DUMPS=MyFileName.dmp
        #note the variables do not work in clang-cl because clang-cl doesn't support vcasan atm.
		target_compile_options(ALL_SANITIZERS INTERFACE /fsanitize=address)
	else()
        #to use more agressive checks make sure to set:
        #ASAN_OPTIONS=strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
        #or define it through __asan_default_options
		target_compile_options(ALL_SANITIZERS INTERFACE -fsanitize=address)
		target_link_options(ALL_SANITIZERS INTERFACE -fsanitize=address)
	endif()
endif()


option(USE_UBSAN "Use undefined behavior sanitizer" OFF)
if(USE_UBSAN)

	if(MSVC) 
        if(NOT FORCE_STATIC_VCRT)
            message(WARNING "atm msvc ubsan requires a static non-debug CRT.")
        endif()
        message(ERROR "atm /fsanitize=undefined does not exist on msvc (it does on clang++), you technically can use UBsan on msvc, but you need to link using the hidden ubsanXXXX.lib file and /MT runtime")
	else()

        if(MY_CLANGCL AND NOT FORCE_STATIC_VCRT)
            message(WARNING "atm clang-cl ubsan requires a static non-debug CRT.")
        endif()

		target_compile_options(ALL_SANITIZERS INTERFACE -fsanitize=undefined)
        #make the application stop
		target_compile_options(ALL_SANITIZERS INTERFACE -fno-sanitize-recover=all)
        target_link_options(ALL_SANITIZERS INTERFACE -fsanitize=undefined)

		#these are clang specific, and these flags cause many false positives (especially in header-only libraries...)
		if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			target_compile_options(ALL_SANITIZERS INTERFACE -fsanitize=unsigned-integer-overflow -fsanitize=nullability -fsanitize=implicit-conversion -fsanitize=local-bounds)
		endif()

        #if portable, it makes sense to raise a trap because a trap will make a core dump.
        #this affects ubsan and clang's CFI santizers, maybe clang's safe-stack santizer?
        if(PORTABLE)
            if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                target_compile_options(ALL_SANITIZERS INTERFACE -fsanitize-trap=undefined)
                target_link_options(ALL_SANITIZERS INTERFACE -fsanitize-trap=undefined)
            else()
                #gcc doesn't support the same flags
                target_compile_options(ALL_SANITIZERS INTERFACE -fsanitize-undefined-trap-on-error)
                target_link_options(ALL_SANITIZERS INTERFACE -fsanitize-undefined-trap-on-error)
            endif()
        else()
            if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                target_compile_options(ALL_SANITIZERS INTERFACE -fno-sanitize-trap=undefined)
                target_link_options(ALL_SANITIZERS INTERFACE -fno-sanitize-trap=undefined)
            else()
                #gcc doesn't support the same flags
                target_compile_options(ALL_SANITIZERS INTERFACE -fno-sanitize-undefined-trap-on-error)
                target_link_options(ALL_SANITIZERS INTERFACE -fno-sanitize-undefined-trap-on-error)
            endif()
        endif()
	endif()
endif ()


# I am unsure if TSAN would work, the FAQ say that C++11 threads are buggy
# or require the libc/libstdc++ standard library to be built with TSAN
option(USE_TSAN "Use thread sanitizer" OFF)
if(USE_TSAN)
    target_compile_options(ALL_SANITIZERS INTERFACE -fsanitize=thread)
    target_link_options(ALL_SANITIZERS INTERFACE -fsanitize=thread)
endif()

#I think MSAN requires all libraries to also be compiled with msan, unlike other sanitizers.
#this includes libstdc++ or libc++ and libraries like SDL2
option(USE_MSAN "Use memory sanitizer" OFF)
if(USE_MSAN)
    target_compile_options(ALL_SANITIZERS INTERFACE -fsanitize=memory -fsanitize-memory-track-origins)
    target_link_options(ALL_SANITIZERS INTERFACE -fsanitize=memory -fsanitize-memory-track-origins)
endif()

option(USE_FORTIFY "Use _FORTIFY_SOURCE=2 on linux" OFF)
if(USE_FORTIFY)
    target_compile_options(ALL_SANITIZERS INTERFACE -D_FORTIFY_SOURCE=2)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

    option(USE_CLANG_SAFESTACK "enable clang's fsanitize=safe-stack (linux only)" OFF)
    if(USE_CLANG_SAFESTACK)
        #one problem with safe-stack is that it doesn't work accross DSO's (maybe?)
        #and it will not work with LSAN
        target_compile_options(ALL_SANITIZERS INTERFACE -fsanitize=safe-stack)
        target_link_options(ALL_SANITIZERS INTERFACE -fsanitize=safe-stack)
    endif()

    option(USE_CLANG_CFI "Use clang's Control Flow Integrety sanitizer (linux only)" OFF)
    if(USE_CFI)
        if(NOT USE_LTO)
            message(WARNING "you must enabled USE_LTO with cfi")
        endif()

        #note I think CMAKE_INTERPROCEDURAL_OPTIMIZATION will actually
        #emit something different than -flto, but I think the llvm IR is all that matters.
        #target_compile_options(ALL_SANITIZERS INTERFACE -flto)
        #target_link_options(ALL_SANITIZERS INTERFACE -flto)

        #visibility hidden is required 
        target_compile_options(ALL_SANITIZERS INTERFACE -fvisibility=hidden)
        
        #on my system, std::to_string would cause an indirect call error...
        #maybe that is libstd++'s fault, maybe libc is needed.
        target_compile_options(ALL_SANITIZERS INTERFACE -fsanitize=cfi)
        target_link_options(ALL_SANITIZERS INTERFACE -fsanitize=cfi)

        if(PORTABLE)
            target_compile_options(ALL_SANITIZERS INTERFACE -fsanitize-trap=cfi)
            target_link_options(ALL_SANITIZERS INTERFACE -fsanitize-trap=cfi)
        else()
            target_compile_options(ALL_SANITIZERS INTERFACE -fno-sanitize-trap=cfi)
            target_link_options(ALL_SANITIZERS INTERFACE -fno-sanitize-trap=cfi)
        endif()
    endif()
endif()

if(WIN32)
    option(USE_MSVC_CFG "enable /guard:cf on msvc" OFF)
    if(USE_MSVC_CFG)
        if(MSVC)
            target_compile_options(ALL_SANITIZERS INTERFACE /guard:cf)
            target_link_options(ALL_SANITIZERS INTERFACE /guard:cf)
        elseif(MY_CLANGCL)
            target_compile_options(ALL_SANITIZERS INTERFACE -Xclang -cfguard)
            target_link_options(ALL_SANITIZERS INTERFACE -Xlinker /guard:cf)
        else()
            #I probably could get it working on msys, I just don't know the flag.
            message(WARNING "/guard:cf unsupported")
        endif()
    endif()
endif()



option(USE_STACK_CLASH_PROTECTION "enable fstack-clash-protection (linux only)" OFF)
if(USE_STACK_CLASH_PROTECTION)
    #stack clash protection might already be enabled by default?
    target_compile_options(ALL_SANITIZERS INTERFACE -fstack-clash-protection)
    target_link_options(ALL_SANITIZERS INTERFACE -fstack-clash-protection)
endif()

option(USE_STACK_PROTECTOR "enable fstack-protector-strong (linux only)" OFF)
if(USE_STACK_PROTECTOR)
    target_compile_options(ALL_SANITIZERS INTERFACE -fstack-protector-strong)
    target_link_options(ALL_SANITIZERS INTERFACE -fstack-protector-strong)
endif()


#
# EMSCRIPTEN
#
if(EMSCRIPTEN)
    #NOTE: for USE_ASAN I need -sINITIAL_MEMORY=159391744, and it only works when I use CMAKE_EXE_LINKER_FLAGS
    #TODO: removing NDEBUG isn't enough to enable asserts for some reason. So don't use RelWithDebInfo, use Debug...
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file ../pointsprite/demo_files@/ --shell-file ../pointsprite/demo_code/wasm_index.html")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sEXPORTED_FUNCTIONS=_main,_enter_fullscreen,_paste_clipboard,_copy_clipboard,_cut_clipboard -sEXPORTED_RUNTIME_METHODS=ccall,cwrap")
    if(USE_OLD_SDL2)
        # emscripten actually uses the newest SDL2 version
        # but I repurpose USE_OLD_SDL2 to equal "let emscripten automatically build SDL2"
        # it's stupid.
        # the reason I do this is because I build SDL2 with MinSizeRel
        # note that you probably want to call emcc --clear-ports to remove the port before building SDL2
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sUSE_SDL=2")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sFULL_ES3")
else()
endif()


#
# start of project
#

add_executable(${PROJECT_NAME}
    code/main.cpp
    code/global_pch.h
    code/global.h
    code/global.cpp
    code/app.h
    code/app.cpp
    code/cvar.h
    code/cvar.cpp
    code/debug_tools.h
    code/debug_tools.cpp
    code/demo.h
    code/demo.cpp
    code/RWops.h
    code/RWops.cpp
    code/console.h
    code/console.cpp
    code/keybind.h
    code/keybind.cpp
    code/ui.h
    code/ui.cpp

    code/options_menu/options_tree.h
    code/options_menu/options_tree.cpp
    code/options_menu/options_list.h
    code/options_menu/options_list.cpp
    code/options_menu/options_cvar_template.h
    code/options_menu/options_cvar_template.cpp
    
    code/shaders/pointsprite.h
    code/shaders/pointsprite.cpp
    code/shaders/basic.h
    code/shaders/basic.cpp
    code/shaders/mono.h
    code/shaders/mono.cpp

    code/font/font_manager.h
    code/font/font_manager.cpp
    code/font/text_prompt.h
    code/font/text_prompt.cpp
    code/font/utf8_stuff.h
    code/font/utf8_stuff.cpp

    code/opengles2/SDL_gles2funcs.h.txt
    code/opengles2/opengl_stuff.h
    code/opengles2/opengl_stuff.cpp
    
    code/BS_Archive/BS_archive.h
    code/BS_Archive/BS_binary.h
    code/BS_Archive/BS_json.h
    code/BS_Archive/BS_json.cpp
    code/BS_Archive/BS_stream.h
    code/BS_Archive/BS_stream.cpp
)

#I'm not using this to speed up compilation,
#this SHOULD help clangd to deal with non self-contained headers 
#because it's automatically #include'd into every file
#but clangd doesn't detect it for some reason...
target_precompile_headers(${PROJECT_NAME} PRIVATE "code/global_pch.h")

target_link_libraries(${PROJECT_NAME} ALL_SANITIZERS)
target_compile_options(${PROJECT_NAME} PRIVATE ${MY_COMPILER_FLAGS})

if(NOT WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:_GLIBCXX_DEBUG>)
endif()

#enable this just in case (clang on linux doesn't use PIE)
option(USE_PIE "makes stronger security" OFF)
include(CheckPIESupported)
check_pie_supported()
set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ${USE_PIE})

#
# This doesn't apply to dumbjson, but I might need this later.
#
if(MSVC)
    option(DISABLE_WIN32_TERM "sets /subsystem:windows, no console output" OFF)
    if(DISABLE_WIN32_TERM)
        set_property(TARGET ${PROJECT_NAME} PROPERTY WIN32_EXECUTABLE true)
        #make the macro DISABLE_WIN32_TERM set
        target_compile_definitions(${PROJECT_NAME} PRIVATE DISABLE_WIN32_TERM)
    endif()
endif()


#
# SDL2
#
option(USE_OLD_SDL2 "for SDL2 older than 2.0.6" OFF)
find_package(SDL2 CONFIG REQUIRED)
if(USE_OLD_SDL2)
    #the old way of linking.
    #note that there is no static linking because
    #this is PURELY for package managers, which usually HATE static libraries.
    target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
else()
    #if find_package worked, but the target is missing,
    #you probably need to USE_OLD_SDL2
    if(NOT STATIC_BUILD)
        target_link_libraries(${PROJECT_NAME} SDL2::SDL2)
    else()
        target_link_libraries(${PROJECT_NAME} SDL2::SDL2-static)
    endif()
    target_link_libraries(${PROJECT_NAME} SDL2::SDL2main)
endif()




option(USE_LIBBACKTRACE "(linux, maybe mingw) to print stack traces instead of trapping" OFF)
if(USE_LIBBACKTRACE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_LIBBACKTRACE)
    #libbacktrace doesn't have a cmake thing
    #but that's fine because it exclusively only works on linux and mingw.
    target_link_libraries(${PROJECT_NAME} "-lbacktrace")

    #need this to print the module name with libbacktrace
    target_link_libraries(${PROJECT_NAME} "-ldl")
endif()

find_package(Freetype REQUIRED)
target_link_libraries(${PROJECT_NAME} Freetype::Freetype)

find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} glm::glm)


#I use a really new version of rapidjson,
#and rapidjson has not bumped their version number past 1.1.0 since a decade,
#and ATM the package system on linux still uses a really old version of rapidjson
#But it is possible to use vcpkg and use the "date" as a version number.
find_package(RapidJSON CONFIG REQUIRED)

#I have to set SYSTEM because there is a -Wmaybe-uninitialized warning...
#target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${RapidJSON_INCLUDE_DIRS})

#for some reason you have to use lowercase rapidjson target...
target_link_libraries(${PROJECT_NAME} rapidjson)

#
# install
#

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

#3.16 black magic install all shared libraries.
#excluding some system paths, on linux I exlucde paths with /lib 
#so that only FetchContent binaries are copied (otherwise you get .so hell)
#warning: this probably will not update stale DLL's, but I should double check.
if(PORTABLE OR WIN32)

    if(WIN32)
        #sets PATH because it includes address sanitizer DLL's... not set by CMAKE_SYSTEM_LIBRARY_PATH
        set(MY_DEPENDENCY_PATHS ${CMAKE_SYSTEM_LIBRARY_PATH})

        #I like to use CMAKE_PREFIX_PATH for prefixes of installed libraries, but I don't want to bundle the garbage
        foreach(_file ${CMAKE_PREFIX_PATH})
            set(MY_DEPENDENCY_PATHS ${MY_DEPENDENCY_PATHS} "${_file}/${CMAKE_INSTALL_LIBDIR}")
        endforeach()

        install(CODE "set(MY_DEPENDENCY_PATHS \"${MY_DEPENDENCY_PATHS}\")")
    else()
        install(CODE "set(MY_DEPENDENCY_PATHS \"\")")
    endif()
    
    # Transfer the value of ${MY_DEPENDENCY_PATHS} into the install script
	install(CODE "set(MY_DESTINATION_PATH \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")")
    install(CODE "set(MY_EXECUTABLE_NAME \"$<TARGET_FILE:${PROJECT_NAME}>\")")
    install(CODE [[
		file(GET_RUNTIME_DEPENDENCIES
			EXECUTABLES ${MY_EXECUTABLE_NAME}
			RESOLVED_DEPENDENCIES_VAR _r_deps
			UNRESOLVED_DEPENDENCIES_VAR _u_deps
			DIRECTORIES ${MY_DEPENDENCY_PATHS} $ENV{PATH}
			PRE_EXCLUDE_REGEXES "api-ms-*" "ext-ms-*"
			POST_EXCLUDE_REGEXES "^C:/Windows/System32/.*\\.dll" "^/lib/.*\\.so" "^/usr/lib/.*\\.so"
		)
		foreach(_file ${_r_deps})
			if(WIN32)
				#this fixes the filename being lowercase.
				get_filename_component(_file "${_file}" ABSOLUTE)
                #NOTE: I could try to replace the extension with pdb and install that too.
			endif()
			file(INSTALL
				DESTINATION ${MY_DESTINATION_PATH}
				TYPE SHARED_LIBRARY
				FOLLOW_SYMLINK_CHAIN
				FILES "${_file}"
			)
		endforeach()
        foreach(_file ${_u_deps})
            message(WARNING "Unresolved dependency: ${_file}")
		endforeach()
    ]])

    # no point in distributing pdb files because the pdb files require .obj files to remain in place
    # which was a optimization in vs2017 that makes building debug builds much faster....
    # you can use a tool that turns pdb files into full debug
    if(0)
    #if(MSVC AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        #copy the pdb files so the debugger can see them.
        install(CODE "set(MY_DEBUG_SYMBOL $<TARGET_PDB_FILE:${PROJECT_NAME}>)")
        install(CODE [[file(INSTALL FILES ${MY_DEBUG_SYMBOL}  DESTINATION ${MY_DESTINATION_PATH})]])
        #install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" OPTIONAL)
    endif()

endif()
