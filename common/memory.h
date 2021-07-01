/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

//
// memory.h
//

// Constants
#define Mem_AllocA(size)								_alloca((size))

#define Mem_CreatePool(name)							_Mem_CreatePool((name),__FILE__,__LINE__)
#define Mem_DeletePool(pool)							_Mem_DeletePool((pool),__FILE__,__LINE__)

#define Mem_Free(ptr)									_Mem_Free((ptr),__FILE__,__LINE__)
#define Mem_FreeTag(pool,tagNum)						_Mem_FreeTag((pool),(tagNum),__FILE__,__LINE__)
#define Mem_FreePool(pool)								_Mem_FreePool((pool),__FILE__,__LINE__)
#define Mem_Alloc(size)									_Mem_Alloc((size),com_genericPool,0,__FILE__,__LINE__)
#define Mem_PoolAlloc(size,pool,tagNum)					_Mem_Alloc((size),(pool),(tagNum),__FILE__,__LINE__)
#define Mem_ReAlloc(ptr,newSize)						_Mem_ReAlloc((ptr),(newSize),__FILE__,__LINE__)

#define Mem_StrDup(in)									_Mem_PoolStrDup((in),com_genericPool,0,__FILE__,__LINE__)
#define Mem_PoolStrDup(in,pool,tagNum)					_Mem_PoolStrDup((in),(pool),(tagNum),__FILE__,__LINE__)
#define Mem_PoolSize(pool)								_Mem_PoolSize((pool))
#define Mem_TagSize(pool,tagNum)						_Mem_TagSize((pool),(tagNum))
#define Mem_ChangeTag(pool,tagFrom,tagTo)				_Mem_ChangeTag((pool),(tagFrom),(tagTo))

#define Mem_CheckPoolIntegrity(pool)					_Mem_CheckPoolIntegrity((pool),__FILE__,__LINE__)
#define Mem_CheckGlobalIntegrity()						_Mem_CheckGlobalIntegrity(__FILE__,__LINE__)

#define Mem_TouchPool(pool)								_Mem_TouchPool((pool),__FILE__,__LINE__)
#define Mem_TouchGlobal()								_Mem_TouchGlobal(__FILE__,__LINE__)

// Functions
struct memPool_t *_Mem_CreatePool(const char *name, const char *fileName, const int fileLine);
uint32		_Mem_DeletePool(struct memPool_t *pool, const char *fileName, const int fileLine);

uint32		_Mem_Free(const void *ptr, const char *fileName, const int fileLine);
uint32		_Mem_FreeTag(struct memPool_t *pool, const int tagNum, const char *fileName, const int fileLine);
uint32		_Mem_FreePool(struct memPool_t *pool, const char *fileName, const int fileLine);
void		*_Mem_Alloc(size_t size, struct memPool_t *pool, const int tagNum, const char *fileName, const int fileLine);
void		*_Mem_ReAlloc(void *ptr, size_t newSize, const char *fileName, const int fileLine);

char		*_Mem_PoolStrDup(const char *in, struct memPool_t *pool, const int tagNum, const char *fileName, const int fileLine);
uint32		_Mem_PoolSize(struct memPool_t *pool);
uint32		_Mem_TagSize(struct memPool_t *pool, const int tagNum);
uint32		_Mem_ChangeTag(struct memPool_t *pool, const int tagFrom, const int tagTo);

void		_Mem_CheckPoolIntegrity(struct memPool_t *pool, const char *fileName, const int fileLine);
void		_Mem_CheckGlobalIntegrity(const char *fileName, const int fileLine);

void		_Mem_TouchPool(struct memPool_t *pool, const char *fileName, const int fileLine);
void		_Mem_TouchGlobal(const char *fileName, const int fileLine);

void		Mem_Register();
void		Mem_Init();

// But allow these!
inline void *operator new(size_t Size, struct memPool_t *Pool, const int TagNum, const char *FileName, const int FileLine)
{
	return _Mem_Alloc(Size, Pool, TagNum, FileName, FileLine);
}
inline void operator delete(void *Pointer, struct memPool_t *Pool, const int TagNum, const char *FileName, const int FileLine)
{
	_Mem_Free(Pointer, FileName, FileLine);
}
#define QNew(Pool,TagNum) new((Pool),(TagNum),__FILE__,__LINE__)

inline void operator delete(void *Pointer, const char *FileName, const int FileLine)
{
	_Mem_Free(Pointer, FileName, FileLine);
}
#define QDelete delete(__FILE__,__LINE__)
