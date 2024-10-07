#include "loader.h"


uint8_t unalignedGet8(void* src) {
  uintptr_t csrc = (uintptr_t)src;
  uint32_t v = *(uint32_t*)(csrc & 0xfffffffc);
  v = (v >> (((uint32_t)csrc & 0x3) * 8)) & 0x000000ff;
  return v;
}

void unalignedSet8(void* dest, uint8_t value) {
  uintptr_t cdest = (uintptr_t)dest;
  uint32_t d = *(uint32_t*)(cdest & 0xfffffffc);
  uint32_t v = value;
  v = v << ((cdest & 0x3) * 8);
  d = d & ~(0x000000ff << ((cdest & 0x3) * 8));
  d = d | v;
  *(uint32_t*)(cdest & 0xfffffffc) = d;
}

uint32_t unalignedGet32(void* src) {
  uint32_t d = 0;
  uintptr_t csrc = (uintptr_t)src;
  for (int n = 0; n < 4; n++) {
    uint32_t v = unalignedGet8((void*)csrc);
    v = v << (n * 8);
    d = d | v;
    csrc++;
  }
  return d;
}

void unalignedSet32(void* dest, uint32_t value) {
  uintptr_t cdest = (uintptr_t)dest;
  for (int n = 0; n < 4; n++) {
    unalignedSet8((void*)cdest, value & 0x000000ff);
    value = value >> 8;
    cdest++;
  }
}

void unalignedCpy(void* dest, void* src, size_t n) {
  uintptr_t csrc = (uintptr_t)src;
  uintptr_t cdest = (uintptr_t)dest;
  while (n > 0) {
    uint8_t v = unalignedGet8((void*)csrc);
    unalignedSet8((void*)cdest, v);
    csrc++;
    cdest++;
    n--;
  }
}
int readSection(ELFLoaderContext_t* ctx, int n, Elf32_Shdr* h, char* name, size_t name_len) {
  off_t offset = ctx->e_shoff + n * sizeof(Elf32_Shdr);
  LOADER_GETDATA(ctx, offset, h, sizeof(Elf32_Shdr));

  if (h->sh_name) {
    offset = ctx->shstrtab_offset + h->sh_name;
    LOADER_GETDATA(ctx, offset, name, name_len);
  }
  return 0;
}
int readSymbol(ELFLoaderContext_t* ctx, int n, Elf32_Sym* sym, char* name, size_t nlen) {
  off_t pos = ctx->symtab_offset + n * sizeof(Elf32_Sym);
  LOADER_GETDATA(ctx, pos, sym, sizeof(Elf32_Sym))
  if (sym->st_name) {
    off_t offset = ctx->strtab_offset + sym->st_name;
    LOADER_GETDATA(ctx, offset, name, nlen);
  } else {
    Elf32_Shdr shdr;
    return readSection(ctx, sym->st_shndx, &shdr, name, nlen);
  }
  return 0;
#ifdef __linux
err:
  return -1;
#endif
}


/*** Relocation functions ***/


const char* type2String(int symt) {
#define STRCASE(name) \
  case name: return #name;
  switch (symt) {
    STRCASE(R_XTENSA_NONE)
    STRCASE(R_XTENSA_32)
    STRCASE(R_XTENSA_ASM_EXPAND)
    STRCASE(R_XTENSA_SLOT0_OP)
    default:
      return "R_<unknow>";
  }
#undef STRCASE
}


int relocateSymbol(Elf32_Addr relAddr, int type, Elf32_Addr symAddr, Elf32_Addr defAddr, uint32_t* from, uint32_t* to) {
  if (symAddr == 0xffffffff) {
    if (defAddr == 0x00000000) {
      return -1;
    } else {
      symAddr = defAddr;
    }
  }
  switch (type) {
    case R_XTENSA_32:
      {
        *from = unalignedGet32((void*)relAddr);
        *to = symAddr + *from;
        unalignedSet32((void*)relAddr, *to);
        break;
      }
    case R_XTENSA_SLOT0_OP:
      {
        uint32_t v = unalignedGet32((void*)relAddr);
        *from = v;

        /* *** Format: L32R *** */
        if ((v & 0x00000F) == 0x000001) {
          int32_t delta = symAddr - ((relAddr + 3) & 0xfffffffc);
          if (delta & 0x0000003) {
            return -1;
          }
          delta = delta >> 2;
          unalignedSet8((void*)(relAddr + 1), ((uint8_t*)&delta)[0]);
          unalignedSet8((void*)(relAddr + 2), ((uint8_t*)&delta)[1]);
          *to = unalignedGet32((void*)relAddr);
          break;
        }

        /* *** Format: CALL *** */
        /* *** CALL0, CALL4, CALL8, CALL12, J *** */
        if ((v & 0x00000F) == 0x000005) {
          int32_t delta = symAddr - ((relAddr + 4) & 0xfffffffc);
          if (delta & 0x0000003) {
            return -1;
          }
          delta = delta >> 2;
          delta = delta << 6;
          delta |= unalignedGet8((void*)(relAddr + 0));
          unalignedSet8((void*)(relAddr + 0), ((uint8_t*)&delta)[0]);
          unalignedSet8((void*)(relAddr + 1), ((uint8_t*)&delta)[1]);
          unalignedSet8((void*)(relAddr + 2), ((uint8_t*)&delta)[2]);
          *to = unalignedGet32((void*)relAddr);
          break;
        }

        /* *** J *** */
        if ((v & 0x00003F) == 0x000006) {
          int32_t delta = symAddr - (relAddr + 4);
          delta = delta << 6;
          delta |= unalignedGet8((void*)(relAddr + 0));
          unalignedSet8((void*)(relAddr + 0), ((uint8_t*)&delta)[0]);
          unalignedSet8((void*)(relAddr + 1), ((uint8_t*)&delta)[1]);
          unalignedSet8((void*)(relAddr + 2), ((uint8_t*)&delta)[2]);
          *to = unalignedGet32((void*)relAddr);
          break;
        }

        /* *** Format: BRI8  *** */
        /* *** BALL, BANY, BBC, BBCI, BBCI.L, BBS,  BBSI, BBSI.L, BEQ, BGE,  BGEU, BLT, BLTU, BNALL, BNE,  BNONE, LOOP,  *** */
        /* *** BEQI, BF, BGEI, BGEUI, BLTI, BLTUI, BNEI,  BT, LOOPGTZ, LOOPNEZ *** */
        if (((v & 0x00000F) == 0x000007) || ((v & 0x00003F) == 0x000026) || ((v & 0x00003F) == 0x000036 && (v & 0x0000FF) != 0x000036)) {
          int32_t delta = symAddr - (relAddr + 4);
          unalignedSet8((void*)(relAddr + 2), ((uint8_t*)&delta)[0]);
          *to = unalignedGet32((void*)relAddr);
          if ((delta < -(1 << 7)) || (delta >= (1 << 7))) {
            return -1;
          }
          break;
        }

        /* *** Format: BRI12 *** */
        /* *** BEQZ, BGEZ, BLTZ, BNEZ *** */
        if ((v & 0x00003F) == 0x000016) {
          int32_t delta = symAddr - (relAddr + 4);
          delta = delta << 4;
          delta |= unalignedGet32((void*)(relAddr + 1));
          unalignedSet8((void*)(relAddr + 1), ((uint8_t*)&delta)[0]);
          unalignedSet8((void*)(relAddr + 2), ((uint8_t*)&delta)[1]);
          *to = unalignedGet32((void*)relAddr);
          delta = symAddr - (relAddr + 4);
          if ((delta < -(1 << 11)) || (delta >= (1 << 11))) {
            return -1;
          }
          break;
        }

        /* *** Format: RI6  *** */
        /* *** BEQZ.N, BNEZ.N *** */
        if ((v & 0x008F) == 0x008C) {
          int32_t delta = symAddr - (relAddr + 4);
          int32_t d2 = delta & 0x30;
          int32_t d1 = (delta << 4) & 0xf0;
          d2 |= unalignedGet32((void*)(relAddr + 0));
          d1 |= unalignedGet32((void*)(relAddr + 1));
          unalignedSet8((void*)(relAddr + 0), ((uint8_t*)&d2)[0]);
          unalignedSet8((void*)(relAddr + 1), ((uint8_t*)&d1)[0]);
          *to = unalignedGet32((void*)relAddr);
          if ((delta < 0) || (delta > 0x111111)) {
            return -1;
          }
          break;
        }
        return -1;
        break;
      }
    case R_XTENSA_ASM_EXPAND:
      {
        *from = unalignedGet32((void*)relAddr);
        *to = unalignedGet32((void*)relAddr);
        break;
      }
    default:
      assert(0);
      return -1;
  }
  return 0;
}


ELFLoaderSection_t* findSection(ELFLoaderContext_t* ctx, int index) {
  for (ELFLoaderSection_t* section = ctx->section; section != NULL; section = section->next) {
    if (section->secIdx == index) {
      return section;
    }
  }
  return NULL;
}


Elf32_Addr findSymAddr(ELFLoaderContext_t* ctx, Elf32_Sym* sym, const char* sName) {
  for (int i = 0; i < ctx->env->exported_size; i++) {
    if (strcmp(ctx->env->exported[i].name, sName) == 0) {
      return (Elf32_Addr)(ctx->env->exported[i].ptr);
    }
  }
  ELFLoaderSection_t* symSec = findSection(ctx, sym->st_shndx);
  if (symSec)
    return ((Elf32_Addr)symSec->data) + sym->st_value;
  return 0xffffffff;
}


int relocateSection(ELFLoaderContext_t* ctx, ELFLoaderSection_t* s) {
  char name[33] = "<unamed>";
  Elf32_Shdr sectHdr;
  if (readSection(ctx, s->relSecIdx, &sectHdr, name, sizeof(name)) != 0) {
    return -1;
  }
  if (!(s->relSecIdx)) {
    return 0;
  }
  if (!(s->data)) {
    return -1;
  }

  int r = 0;
  Elf32_Rela rel;
  size_t relEntries = sectHdr.sh_size / sizeof(rel);
  for (size_t relCount = 0; relCount < relEntries; relCount++) {
    LOADER_GETDATA(ctx, sectHdr.sh_offset + relCount * (sizeof(rel)), &rel, sizeof(rel))
    Elf32_Sym sym;
    char name[33] = "<unnamed>";
    int symEntry = ELF32_R_SYM(rel.r_info);
    int relType = ELF32_R_TYPE(rel.r_info);
    Elf32_Addr relAddr = ((Elf32_Addr)s->data) + rel.r_offset;  // data to be updated adress
    readSymbol(ctx, symEntry, &sym, name, sizeof(name));
    Elf32_Addr symAddr = findSymAddr(ctx, &sym, name) + rel.r_addend;  // target symbol adress
    uint32_t from = 0;
    uint32_t to = 0;
    if (relType == R_XTENSA_NONE || relType == R_XTENSA_ASM_EXPAND) {
      //            MSG("  %08X %04X %04X %-20s %08X          %08X                    %s + %X", rel.r_offset, symEntry, relType, type2String(relType), relAddr, sym.st_value, name, rel.r_addend);
    } else if ((symAddr == 0xffffffff) && (sym.st_value == 0x00000000)) {
      r = -1;
    } else if (relocateSymbol(relAddr, relType, symAddr, sym.st_value, &from, &to) != 0) {
      r = -1;
    }
  }
  return r;
#ifdef __linux
err:
  return -1;
#endif
}


/*** Main functions ***/


void elfLoaderFree(ELFLoaderContext_t* ctx) {
  if (ctx) {
    ELFLoaderSection_t* section = ctx->section;
    ELFLoaderSection_t* next;
    while (section != NULL) {
      if (section->data) {
        free(section->data);
      }
      next = section->next;
      free(section);
      section = next;
    }
    free(ctx);
  }
}


ELFLoaderContext_t* elfLoaderInitLoadAndRelocate(void* fd, const ELFLoaderEnv_t* env) {

  ELFLoaderContext_t* ctx = (ELFLoaderContext_t*)malloc(sizeof(ELFLoaderContext_t));
  assert(ctx);

  memset(ctx, 0, sizeof(ELFLoaderContext_t));
  ctx->fd = fd;
  ctx->env = env;
  {
    Elf32_Ehdr header;
    Elf32_Shdr section;
    /* Load the ELF header, located at the start of the buffer. */
    LOADER_GETDATA(ctx, 0, &header, sizeof(Elf32_Ehdr));

    /* Make sure that we have a correct and compatible ELF header. */
    char ElfMagic[] = { 0x7f, 'E', 'L', 'F', '\0' };
    if (memcmp(header.e_ident, ElfMagic, strlen(ElfMagic)) != 0) {
      goto err;
    }

    /* Load the section header, get the number of entries of the section header, get a pointer to the actual table of strings */
    LOADER_GETDATA(ctx, header.e_shoff + header.e_shstrndx * sizeof(Elf32_Shdr), &section, sizeof(Elf32_Shdr));
    ctx->e_shnum = header.e_shnum;
    ctx->e_shoff = header.e_shoff;
    ctx->shstrtab_offset = section.sh_offset;
  }

  {
    /* Go through all sections, allocate and copy the relevant ones
        ".symtab": segment contains the symbol table for this file
        ".strtab": segment points to the actual string names used by the symbol table
        */
    for (int n = 1; n < ctx->e_shnum; n++) {
      Elf32_Shdr sectHdr;
      char name[33] = "<unamed>";
      if (readSection(ctx, n, &sectHdr, name, sizeof(name)) != 0) {
        goto err;
      }
      if (sectHdr.sh_flags & SHF_ALLOC) {
        if (!sectHdr.sh_size) {

        } else {
          ELFLoaderSection_t* section = (ELFLoaderSection_t*)malloc(sizeof(ELFLoaderSection_t));
          assert(section);
          memset(section, 0, sizeof(ELFLoaderSection_t));
          section->next = ctx->section;
          ctx->section = section;
          if (sectHdr.sh_flags & SHF_EXECINSTR) {
            section->data = LOADER_ALLOC_EXEC(sectHdr.sh_size);
          } else {
            section->data = LOADER_ALLOC_DATA(sectHdr.sh_size);
          }
          if (!section->data) {
            goto err;
          }
          section->secIdx = n;
          section->size = sectHdr.sh_size;
          if (sectHdr.sh_type != SHT_NOBITS) {
            LOADER_GETDATA(ctx, sectHdr.sh_offset, section->data, sectHdr.sh_size);
          } else {
            memset(section->data, 0, sectHdr.sh_size);
          }
          if (strcmp(name, ".text") == 0) {
            ctx->text = section->data;
          }
        }
      } else if (sectHdr.sh_type == SHT_RELA) {
        if (sectHdr.sh_info >= n) {
          goto err;
        }
        ELFLoaderSection_t* section = findSection(ctx, sectHdr.sh_info);
        if (section == NULL) {
        } else {
          section->relSecIdx = n;
        }
      } else {
        if (strcmp(name, ".symtab") == 0) {
          ctx->symtab_offset = sectHdr.sh_offset;
          ctx->symtab_count = sectHdr.sh_size / sizeof(Elf32_Sym);
        } else if (strcmp(name, ".strtab") == 0) {
          ctx->strtab_offset = sectHdr.sh_offset;
        }
      }
    }
    if (ctx->symtab_offset == 0 || ctx->symtab_offset == 0) {
      goto err;
    }
  }

  {
    int r = 0;
    for (ELFLoaderSection_t* section = ctx->section; section != NULL; section = section->next) {
      r |= relocateSection(ctx, section);
    }
    if (r != 0) {
      goto err;
    }
  }
  return ctx;

err:
  elfLoaderFree(ctx);
  return NULL;
}


int elfLoaderSetFunc(ELFLoaderContext_t* ctx, const char* funcname) {
  ctx->exec = 0;
  for (int symCount = 0; symCount < ctx->symtab_count; symCount++) {
    Elf32_Sym sym;
    char name[33] = "<unnamed>";
    if (readSymbol(ctx, symCount, &sym, name, sizeof(name)) != 0) {
      return -1;
    }
    if (strcmp(name, funcname) == 0) {
      Elf32_Addr symAddr = findSymAddr(ctx, &sym, name);
      if (symAddr == 0xffffffff) {
      } else {
        ctx->exec = (void*)symAddr;
      }
    }
  }
  if (ctx->exec == 0) {
    return -1;
  }
  return 0;
}

char* elfLoaderRun(ELFLoaderContext_t* ctx, char* arg, size_t len) {
  if (!ctx->exec) {
    return NULL;
  }
  typedef char* (*func_t)(char*, size_t len);
  func_t func = (func_t)ctx->exec;
  return func(arg, len);
}
void* elfLoaderGetTextAddr(ELFLoaderContext_t* ctx) {
  return ctx->text;
}
