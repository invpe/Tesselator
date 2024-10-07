#ifndef __CLIB_LOADER__
#define __CLIB_LOADER__

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h> 
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "elf.h"
 
typedef struct {
  const char* name; /*!< Name of symbol */
  void* ptr;        /*!< Pointer of symbol in memory */
} ELFLoaderSymbol_t;

typedef struct {
  const ELFLoaderSymbol_t* exported; /*!< Pointer to exported symbols array */
  unsigned int exported_size;        /*!< Elements on exported symbol array */
} ELFLoaderEnv_t;

typedef struct ELFLoaderContext_t ELFLoaderContext_t; 

#define LOADER_ALLOC_EXEC(size) heap_caps_malloc(size, MALLOC_CAP_EXEC | MALLOC_CAP_32BIT)
#define LOADER_ALLOC_DATA(size) heap_caps_malloc(size, MALLOC_CAP_8BIT)
#define LOADER_GETDATA(ctx, off, buffer, size) unalignedCpy(buffer, ctx->fd + off, size);


typedef struct ELFLoaderSection_t {
  void* data;
  int secIdx;
  size_t size;
  off_t relSecIdx;
  struct ELFLoaderSection_t* next;
} ELFLoaderSection_t;

struct ELFLoaderContext_t {
  void* fd;
  void* exec;
  void* text;
  const ELFLoaderEnv_t* env;

  size_t e_shnum;
  off_t e_shoff;
  off_t shstrtab_offset;

  size_t symtab_count;
  off_t symtab_offset;
  off_t strtab_offset;

  ELFLoaderSection_t* section;
};

/* Function prototypes */
uint8_t unalignedGet8(void* src);
void unalignedSet8(void* dest, uint8_t value);
uint32_t unalignedGet32(void* src);
void unalignedSet32(void* dest, uint32_t value);
void unalignedCpy(void* dest, void* src, size_t n);
int readSection(ELFLoaderContext_t* ctx, int n, Elf32_Shdr* h, char* name, size_t name_len);
int readSymbol(ELFLoaderContext_t* ctx, int n, Elf32_Sym* sym, char* name, size_t nlen);
const char* type2String(int symt);
int relocateSymbol(Elf32_Addr relAddr, int type, Elf32_Addr symAddr, Elf32_Addr defAddr, uint32_t* from, uint32_t* to);
ELFLoaderSection_t* findSection(ELFLoaderContext_t* ctx, int index);
Elf32_Addr findSymAddr(ELFLoaderContext_t* ctx, Elf32_Sym* sym, const char* sName);
int relocateSection(ELFLoaderContext_t* ctx, ELFLoaderSection_t* s);
void elfLoaderFree(ELFLoaderContext_t* ctx);
ELFLoaderContext_t* elfLoaderInitLoadAndRelocate(void* fd, const ELFLoaderEnv_t* env);
int elfLoaderSetFunc(ELFLoaderContext_t* ctx, const char* funcname);
char* elfLoaderRun(ELFLoaderContext_t* ctx, char* arg, size_t len);
void* elfLoaderGetTextAddr(ELFLoaderContext_t* ctx);

#endif /* __CLIB_LOADER__ */
