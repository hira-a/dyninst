# CMake configuration for symtabAPI directory

include_directories (
  src
  h
  ${PROJECT_SOURCE_DIR}/dwarf/h
  ${PROJECT_SOURCE_DIR}/elf/h
  )

if(NOT WIN32)
  include_directories(${PROJECT_SOURCE_DIR}/common/src)
endif()



set (SRC_LIST
                src/Object.C 
                src/Aggregate.C 
                src/Function.C 
                src/Variable.C 
                src/Symbol.C 
                src/LineInformation.C 
                src/Symtab.C 
                src/Symtab-edit.C 
                src/Symtab-lookup.C 
                src/Symtab-deprecated.C 
                src/Module.C 
                src/Region.C 
                src/Collections.C 
                src/Type.C 
                src/AddrLookup.C 
                src/annotations.C 
                src/debug.C 
                src/SymtabReader.C 
  )

if (PLATFORM MATCHES freebsd OR 
    PLATFORM MATCHES linux OR 
    PLATFORM MATCHES bgq OR
    PLATFORM MATCHES cnl)

set (SRC_LIST ${SRC_LIST}
    src/Object-elf.C 
    src/Archive.C 
    src/Archive-elf.C 
    src/parseDwarf.C
        src/parseStab.C
        src/LinkMap.C
    src/emitElfStatic.C 
    src/dwarfWalker.C
)

if (PLATFORM MATCHES x86_64 OR PLATFORM MATCHES amd64)
set (SRC_LIST ${SRC_LIST}
    src/emitElf-64.C
    src/emitElfStatic-x86.C 
    src/relocationEntry-elf-x86.C
    )
elseif (PLATFORM MATCHES i386)
set (SRC_LIST ${SRC_LIST}
    src/emitElfStatic-x86.C
    src/relocationEntry-elf-x86.C
    )
elseif (PLATFORM MATCHES ppc32)
set (SRC_LIST ${SRC_LIST}
    src/emitElfStatic-ppc32.C 
    src/relocationEntry-elf-ppc32.C
    )
elseif (PLATFORM MATCHES ppc64)
set (SRC_LIST ${SRC_LIST}
    src/emitElf-64.C
    src/emitElfStatic-ppc64.C
    src/relocationEntry-elf-ppc64.C
    )
elseif (PLATFORM MATCHES aarch64)
set (SRC_LIST ${SRC_LIST}
    src/emitElf-64.C
    src/emitElfStatic-aarch64.C
    src/relocationEntry-elf-aarch64.C
    )
else (TRUE)
set (SRC_LIST ${SRC_LIST}
    src/emitElf-64.C
    src/emitElfStatic-stub.C
    src/relocationEntry-stub.C
    )
endif()
endif()


if (PLATFORM MATCHES nt)
set (SRC_LIST ${SRC_LIST}
    src/Object-nt.C
	src/emitWin.C
	src/relocationEntry-stub.C
)
endif()

SET_SOURCE_FILES_PROPERTIES(${SRC_LIST} PROPERTIES LANGUAGE CXX)

ADD_DEFINITIONS(-DSYMTAB_LIB)

set (DEPS common)

if(UNIX)
  set (DEPS ${DEPS}
    dynElf ${LIBELF_LIBRARIES}
    dynDwarf ${LIBDWARF_LIBRARIES}
    )
else()
  set (DEPS ${DEPS} dbghelp)
endif()

dyninst_library(symtabAPI ${DEPS})

