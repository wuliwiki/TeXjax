# use g++ with libraries
# requires g++8.3 or higher
# any change in *.h.in file will cause all *.cpp files to compile
# to debug only one file, e.g. test_a.cpp, use `make test_a.o link`

#======== options =========
# compiler [g++|clang++|icpc|icpx]
opt_compiler = g++
# debug mode
opt_debug = false
# address sanitizer (only for g++ dynamic debug build)
opt_asan = false # $(opt_debug)
# c++ standard [c++11|gnu++11]
opt_std = c++11
# static link (not all libs supported)
opt_static = true
# use Boost lib
opt_boost = false
# use SQLiteCpp
opt_sqlitecpp = true
#==========================

$(info ) $(info ) $(info ) $(info ) $(info ) $(info )

# === Debug / Release ===
ifeq ($(opt_debug), true)
    $(info Build: Debug)
    debug_flag = -g
    ifeq ($(opt_compiler), g++)
        debug_flag = -g -ftrapv $(asan_flag)
    endif
else
    $(info Build: Release)
    release_flag = -O3
endif

# Address Sanitizer
ifeq ($(opt_asan), true)
    ifeq ($(opt_compiler), g++)
        ifeq ($(opt_static), false)
            $(info Address Sanitizer: on)
            asan_flag = -fsanitize=address -static-libasan
        else
            $(info Address Sanitizer: off)
        endif
    else
        $(info Address Sanitizer: off)
    endif
else
    $(info Address Sanitizer: off)
endif

# === Boost ===
ifeq ($(opt_boost), true)
    ifeq ($(opt_static), false)
        $(info Boost: dynamic)
        boost_lib = -l:libboost_system.so -l:libboost_filesystem.so
    else
        $(info Boost: static)
        boost_lib = -l:libboost_system.a -l:libboost_filesystem.a
    endif
else
    $(info Boost: off)
endif

# === SQLiteCpp ===
ifeq ($(opt_sqlitecpp), true)
    opt_sqlite = false
    $(info SQLiteCpp: static)
    sqlitecpp_lib = -l:libSQLiteCpp.a -l:libSQLiteCpp-sqlite3.a
else
    $(info SQLiteCpp: off)
endif


# === compiler flags ===
ifeq ($(opt_compiler), g++)
    compiler_flag = -std=$(opt_std) -Wall -Wno-reorder -fmax-errors=5 -fopenmp
endif

$(info  )$(info  )$(info  )$(info  )
# ---------------------------------------------------------

# all flags
flags = $(compiler_flag) $(debug_flag) $(release_flag) $(mkl_flag) $(boost_flag) $(arb_flag) $(sqlitecpp_flag)
# -pedantic # show more warnings

# all libs
ifeq ($(opt_static), true)
    static_flag = -static
endif
libs = $(static_flag) $(mplapack_lib) $(boost_lib) $(sqlitecpp_lib) -l pthread -l dl

# default target
PhysWikiScan: main.o # link
	@printf "\n\n   --- link ---\n\n"
	$(opt_compiler) $(flags) -o PhysWikiScan main.o $(libs)

link: # force link
	$(opt_compiler) $(flags) -o PhysWikiScan main.o $(libs)

clean:
	rm -f *.o PhysWikiScan

main.o: main.cpp lib/*.h SLISC/*.h SLISC/*/*.h
	$(opt_compiler) $(flags) -c main.cpp
