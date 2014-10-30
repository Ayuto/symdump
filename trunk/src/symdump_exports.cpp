/**
* =============================================================================
* binutils
* Copyright(C) 2013 Ayuto. All rights reserved.
* =============================================================================
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License, version 3.0, as published by the
* Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
* details.
*
* You should have received a copy of the GNU General Public License along with
* this program.  If not, see <http://www.gnu.org/licenses/>.
**/

// ============================================================================
// >> INCLUDES
// ============================================================================
// Python
#include "Python.h"

// C/C++
#include <stdio.h>
#ifdef __linux__
    #include <fcntl.h>
    #include <link.h>
    #include <sys/mman.h>
#endif


// ============================================================================
// >> GLOBAL VARIABLES
// ============================================================================
char* g_szAddrType = "k";


// ============================================================================
// >> EXPORTED FUNCTIONS
// ============================================================================
static PyObject* setAddrType(PyObject* self, PyObject* args)
{
    char* szAddrType = NULL;
    if (!PyArg_ParseTuple(args, "s", &szAddrType))
        Py_RETURN_NONE;

    if (strlen(szAddrType) != 1)
        return PyErr_Format(PyExc_ValueError, "Expected string of size 1");

    g_szAddrType = szAddrType;
    Py_RETURN_NONE;
}

static PyObject* dumpSymbols(PyObject* self, PyObject* args)
{
#ifdef __linux__
    // Parse and check arguments
    char* szPath = NULL;
    if (!PyArg_ParseTuple(args, "s", &szPath))
        Py_RETURN_NONE;

    void* pAddr = dlopen(szPath, RTLD_NOW);
    if (!pAddr)
        Py_RETURN_NONE;

    dlclose(pAddr);

    // Start searching for symbols
    struct link_map *dlmap;
    struct stat dlstat;
    int dlfile;
    uintptr_t map_base;
    Elf32_Ehdr *file_hdr;
    Elf32_Shdr *sections, *shstrtab_hdr, *symtab_hdr, *strtab_hdr;
    Elf32_Sym *symtab;
    const char *shstrtab, *strtab;
    uint16_t section_count;
    uint32_t symbol_count;

    dlmap = (struct link_map *) pAddr;
    symtab_hdr = NULL;
    strtab_hdr = NULL;

    dlfile = open(dlmap->l_name, O_RDONLY);
    if (dlfile == -1 || fstat(dlfile, &dlstat) == -1)
    {
        close(dlfile);
        Py_RETURN_NONE;
    }

    // Map library file into memory
    file_hdr = (Elf32_Ehdr *)mmap(NULL, dlstat.st_size, PROT_READ, MAP_PRIVATE, dlfile, 0);
    map_base = (uintptr_t)file_hdr;
    if (file_hdr == MAP_FAILED)
    {
        close(dlfile);
        Py_RETURN_NONE;
    }
    close(dlfile);

    if (file_hdr->e_shoff == 0 || file_hdr->e_shstrndx == SHN_UNDEF)
    {
        munmap(file_hdr, dlstat.st_size);
        Py_RETURN_NONE;
    }

    sections = (Elf32_Shdr *)(map_base + file_hdr->e_shoff);
    section_count = file_hdr->e_shnum;
    // Get ELF section header string table
    shstrtab_hdr = &sections[file_hdr->e_shstrndx];
    shstrtab = (const char *)(map_base + shstrtab_hdr->sh_offset);

    // Iterate sections while looking for ELF symbol table and string table
    for (uint16_t i = 0; i < section_count; i++)
    {
        Elf32_Shdr &hdr = sections[i];
        const char *section_name = shstrtab + hdr.sh_name;

        if (strcmp(section_name, ".symtab") == 0)
            symtab_hdr = &hdr;

        else if (strcmp(section_name, ".strtab") == 0)
            strtab_hdr = &hdr;
    }

    // Uh oh, we don't have a symbol table or a string table
    if (symtab_hdr == NULL || strtab_hdr == NULL)
    {
        munmap(file_hdr, dlstat.st_size);
        Py_RETURN_NONE;
    }

    symtab = (Elf32_Sym *)(map_base + symtab_hdr->sh_offset);
    strtab = (const char *)(map_base + strtab_hdr->sh_offset);
    symbol_count = symtab_hdr->sh_size / symtab_hdr->sh_entsize;

    // Create a Python dictionary
    PyObject* pDict = PyDict_New();

    if (!pDict)
    {
        munmap(file_hdr, dlstat.st_size);
        Py_RETURN_NONE;
    }

    // Iterate symbol table starting from the position we were at last time
    for (uint32_t i = 0; i < symbol_count; i++)
    {
        Elf32_Sym &sym = symtab[i];
        unsigned char sym_type = ELF32_ST_TYPE(sym.st_info);
        const char *sym_name = strtab + sym.st_name;

        // Skip symbols that are undefined or do not refer to functions or objects
        if (sym.st_shndx == SHN_UNDEF || (sym_type != STT_FUNC && sym_type != STT_OBJECT))
            continue;

        // Add the symbol and the address to our dict
        PyObject* pValue = Py_BuildValue(g_szAddrType, (void *)(dlmap->l_addr + sym.st_value));
        if (pValue)
        {
            PyDict_SetItemString(pDict, sym_name, pValue);
            Py_XDECREF(pValue);
        }
    }

    // Unmap the file now.
    munmap(file_hdr, dlstat.st_size);
    return pDict;
#else
    return PyErr_Format(PyExc_NotImplementedError, "dumpSymbols was not implemented on this OS");
#endif // __linux__
}


// ============================================================================
// >> GLOBAL VARIABLES
// ============================================================================
static PyMethodDef g_PyFuncs[] = {
    {"dumpSymbols",  dumpSymbols, METH_VARARGS, "Returns a dictionary containing all symbols and their addresses within a binary"},
    {"setAddrType",  setAddrType, METH_VARARGS, "Sets the type for the address representation (k by default)"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef symdumpmodule = {
   PyModuleDef_HEAD_INIT,
   "symdump",
   NULL,
   -1,
   g_PyFuncs
};

// ============================================================================
// >> INITIALIZATION FUNCTION
// ============================================================================
PyMODINIT_FUNC PyInit_symdump(void)
{
    return PyModule_Create(&symdumpmodule);
}