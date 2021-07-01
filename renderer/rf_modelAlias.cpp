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
// rf_modelAlias.cpp
//

#include "rf_modelLocal.h"

/*
===============================================================================

	MD2 LOADING

===============================================================================
*/

/*
=================
R_CalcAliasNormals

ala Vic
=================
*/
int uniqueVerts[MD2_MAX_VERTS];
int vertRemap[MD2_MAX_VERTS];
vec3_t trNormals[MD2_MAX_TRIANGLES];
static void R_CalcAliasNormals(const int numIndexes, index_t *indexArray, const int numVerts, mAliasVertex_t *vertexes)
{
	// count unique verts
	int numUniqueVerts = 0;
	for (int i=0 ; i<numVerts ; i++)
	{
		bool bFound = false;
		for (int j=0 ; j<numUniqueVerts ; j++)
		{
			if (Vec3Compare(vertexes[uniqueVerts[j]].point, vertexes[i].point))
			{
				vertRemap[i] = j;
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			vertRemap[i] = numUniqueVerts;
			uniqueVerts[numUniqueVerts++] = i;
		}
	}

	for (int i=0, j=0 ; i<numIndexes ; i+=3, j++)
	{
		vec3_t dir1, dir2;

		// calculate two mostly perpendicular edge directions
		Vec3Subtract(vertexes[indexArray[i+0]].point, vertexes[indexArray[i+1]].point, dir1);
		Vec3Subtract(vertexes[indexArray[i+2]].point, vertexes[indexArray[i+1]].point, dir2);

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the surface normal
		CrossProduct(dir1, dir2, trNormals[j]);
		VectorNormalizef(trNormals[j], trNormals[j]);
	}

	// sum all triangle normals
	byte latLongs[MD2_MAX_VERTS][2];
	for (int i=0 ; i<numUniqueVerts ; i++)
	{
		vec3_t normal;
		Vec3Clear(normal);

		for (int j=0, k=0 ; j<numIndexes ; j+=3, k++)
		{
			if (vertRemap[indexArray[j+0]] == i || vertRemap[indexArray[j+1]] == i || vertRemap[indexArray[j+2]] == i)
				Vec3Add(normal, trNormals[k], normal);
		}

		VectorNormalizef(normal, normal);
		NormToLatLong(normal, latLongs[i]);
	}

	// copy normals back
	for (int i=0 ; i<numVerts ; i++)
		*(short *)vertexes[i].latLong = *(short *)latLongs[vertRemap[i]];
}

// File format:
// [string] "EGLCACHE"
// (loop until EOF)
// [uint32] block hash value
// [length-prefixed string] block unique name
// [uint32] data length
// [variable byte] data

// Blocks are always added to the end.
// Removing blocks from the cache is the most expensive operation.
static const char *CacheHeader = "EGLCACHE";

class CacheBlock;
class CacheStorage;

class CacheStorage
{
private:
	fileHandle_t handle;
	uint32 len;
	String _fileName;

	// re-seeks to data start and resets length
	void Reset ()
	{
		FS_Seek (handle, 8, FS_SEEK_SET);
		RecalculateLength();
	}

	void RecalculateLength()
	{
		len = FS_FileLength(handle);
	}

	friend class CacheBlock;

	template<typename T>
	T ReadSimpleType ()
	{
		T v;
		FS_Read(&v, sizeof(v), handle);
		return v;
	}

	void ReadBuffer (void *buffer, const uint32 length)
	{
		FS_Read(buffer, length, handle);
	}

	String ReadStringLength (uint32 length)
	{
		String str;

		for (uint32 i = 0; i < length; ++i)
			str += ReadSimpleType<char>();

		return str;
	}

	template<typename T>
	void WriteSimpleType (const T &value)
	{
		FS_Write((void*)&value, sizeof(value), handle);
	}

	template <typename T>
	void WriteArray (const T *array, const uint32 length)
	{
		FS_Write((void*)array, sizeof(T) * length, handle);
	}

	void WriteStringLength (const String &string)
	{
		WriteSimpleType<uint32>(string.Count());
		WriteArray<char>(string.CString(), string.Count());
	}

	void SkipBytes (uint32 bytes)
	{
		static byte skipBuf[512];

		while (bytes > 0)
		{
			FS_Read(&skipBuf, (bytes > 512) ? 512 : bytes, handle);
			
			if (bytes < 512)
				break;

			bytes -= 512;
		}
	}

public:
	bool IsEOF ()
	{
		return FS_Tell(handle) == len;
	}

	CacheStorage (const String &fileName)
	{
		Open (fileName);
	}

	void Open (const String &fileName)
	{
		_fileName = fileName;

		if (FS_FileExists(fileName.CString()) == -1)
		{
			FS_OpenFile(fileName.CString(), &handle, FS_MODE_WRITE_BINARY);
			FS_CloseFile(handle);
		}

		len = FS_OpenFile(fileName.CString(), &handle, FS_MODE_READ_WRITE_BINARY);
		
		// check header
		if (len == 0)
		{
			WriteArray<char>(CacheHeader, 8);
			len = 8;
		}
		else
		{
			char header[8];
			FS_Read(&header, 8, handle);

			if (Q_strnicmp(header, CacheHeader, 8) != 0)
				throw Exception();
		}
	}

	void Close ()
	{
		FS_CloseFile(handle);
	}

	// returns block position in file, < 8 means no match.
	uint32 FindBlock (const char *UID);

	// deletes a block
	// block is invalidated after this.
	void DeleteBlock (CacheBlock &block);

	// Get a block for reading.
	// If offset is < 8, block is invalid;
	CacheBlock GetBlock (uint32 offset);

	// Returns a new block for writing.
	CacheBlock NewBlock ();

	// Create or find a new block.
	CacheBlock FindOrCreateBlock (const char *UID, bool &created);

	// Only create block (performance increase)
	CacheBlock CreateBlock (const char *UID);
};

enum CacheType
{
	CACHE_READONLY	= 1,
	CACHE_WRITEONLY	= 2,
	CACHE_NEITHER
};

class ExceptionCacheOperationNotSupported : Exception
{
public:
	ExceptionCacheOperationNotSupported() :
	  Exception("The operation is not supported in the caches' current state.")
	  {
	  };
};

class CacheBlock
{
protected:
	CacheStorage *_cache;
	uint32 _position;

	uint32 _idHash;
	String _id;
	uint32 _length;

	// for writing
	TList<byte> _memoryBuffer;

	CacheType _type;

	CacheBlock();
	CacheBlock(CacheStorage *cache, bool newBlock);

	friend class CacheStorage;

	static CacheBlock _invalidBlock;

public:
	String &GetID() { return _id; }

	inline bool IsWritable () { return !!(_type & CACHE_WRITEONLY); }
	inline bool IsReadable () { return !!(_type & CACHE_READONLY); }

	// get real block length, including id/string/hash
	inline uint32 BlockLength ()
	{
		return sizeof(uint32) + sizeof(uint32) + _id.Count() + sizeof(uint32) + _length;
	}

	inline bool IsValid ()
	{
		return (this == &_invalidBlock);
	}

	template<typename T>
	void Write (const T &value)
	{
		if (!IsWritable())
			throw ExceptionCacheOperationNotSupported();

		union
		{
			T value;
			byte bytes[sizeof(T)];
		} wv = {value};

		_memoryBuffer.AddRange(wv.bytes, sizeof(T));
	}

	void WriteBuffer (const void *data, const uint32 length)
	{
		if (!IsWritable())
			throw ExceptionCacheOperationNotSupported();

		_memoryBuffer.AddRange((byte*)data, length);
	}

	template<typename T>
	inline T ReadSimpleType ()
	{
		if (!IsReadable())
			throw ExceptionCacheOperationNotSupported();

		return _cache->ReadSimpleType<T>();
	}

	inline void ReadBuffer (void *buffer, const uint32 length)
	{
		if (!IsReadable())
			throw ExceptionCacheOperationNotSupported();

		_cache->ReadBuffer(buffer, length);
	}

	inline String ReadStringLength (uint32 length)
	{
		if (!IsReadable())
			throw ExceptionCacheOperationNotSupported();

		return _cache->ReadStringLength(length);
	}

	// Flush written data to handle
	void Flush ();
};

/*static*/ CacheBlock CacheBlock::_invalidBlock;

CacheBlock::CacheBlock () :
  _type(CACHE_NEITHER)
{
};

CacheBlock::CacheBlock (CacheStorage *cache, bool newBlock) :
	_cache(cache),
	_type(newBlock ? CACHE_WRITEONLY : CACHE_READONLY),
	_position(FS_Tell(cache->handle)),
	_memoryBuffer(128)
{
	if (!newBlock)
	{
		_idHash = _cache->ReadSimpleType<uint32>();
		_id = _cache->ReadStringLength(_cache->ReadSimpleType<uint32>());
		_length = _cache->ReadSimpleType<uint32>();
	}
	else
		_id = String::Empty();
};

void CacheBlock::Flush()
{
	_cache->WriteSimpleType<uint32>(std::hash<std::string>()(_id.CString()));
	_cache->WriteStringLength(_id);
	_cache->WriteSimpleType<uint32>(_memoryBuffer.Count());
	_cache->WriteArray(_memoryBuffer.Array(), _memoryBuffer.Count()); 
	_memoryBuffer.Clear();

	_cache->Reset();
}

// returns block position in file, < 8 means no match.
uint32 CacheStorage::FindBlock (const char *UID)
{
	Reset();

	uint32 realHash = std::hash<std::string>()(UID);
	uint32 blockNum = 0;

	while (!IsEOF())
	{
		CacheBlock block (this, false);

		if (block._idHash == realHash)
		{
			if (block._id.Compare(UID) == 0)
				return block._position;
			else
				SkipBytes(block._length);
		}
		else
			SkipBytes(block._length);
	}

	return 0;
}

CacheBlock CacheStorage::FindOrCreateBlock (const char *UID, bool &created)
{
	uint32 pos = FindBlock(UID);
	created = false;

	if (!pos)
	{
		created = true;

		CacheBlock block = NewBlock();
		block.GetID() = UID;
		return block;
	}

	return GetBlock(pos);
}

CacheBlock CacheStorage::CreateBlock (const char *UID)
{
	CacheBlock block = NewBlock();
	block.GetID() = UID;
	return block;
}

inline void CopyAppendBytes (const char *src, const char *dst, const uint32 position, const uint32 len)
{
	fileHandle_t srcHandle, dstHandle;

	FS_OpenFile(src, &srcHandle, FS_MODE_READ_BINARY);
	FS_OpenFile(dst, &dstHandle, FS_MODE_APPEND_BINARY);

	if (!srcHandle || !dstHandle)
	{
		if (srcHandle)
			FS_CloseFile(srcHandle);
		
		if (dstHandle)
			FS_CloseFile(dstHandle);

		throw Exception();
	}

	FS_Seek(srcHandle, position, FS_SEEK_SET);

	byte buffer[4096];
	uint32 realLen = len;

	while (realLen)
	{
		int readLen = FS_Read (buffer, ((realLen > sizeof(buffer)) ? sizeof(buffer) : realLen), srcHandle);
		FS_Write(buffer, readLen, dstHandle);

		realLen -= readLen;
	}

	FS_CloseFile(srcHandle);
	FS_CloseFile(dstHandle);
}

// deletes a block
// all blocks are invalidated after this.
// !FIXME: Find a way to make ALL blocks invalidate...
void CacheStorage::DeleteBlock (CacheBlock &block)
{
	Close();

	var backupName = (_fileName + ".bak");
	FS_RenameFile((String(BASE_MODDIRNAME"/") + _fileName).CString(), (String(BASE_MODDIRNAME"/") + backupName).CString());

	uint32 len = FS_FileLength(backupName.CString());
	uint32 endOfBlock = block._position + block.BlockLength();
	uint32 restLen = len - endOfBlock;

	CopyAppendBytes (backupName.CString(), _fileName.CString(), 0, block._position);
	CopyAppendBytes (backupName.CString(), _fileName.CString(), endOfBlock, restLen);

	FS_DeleteFile(backupName.CString());

	Open(_fileName);

	block = CacheBlock::_invalidBlock;
}

// Get a block for reading.
// If offset is < 8, block is invalid;
CacheBlock CacheStorage::GetBlock (uint32 offset)
{
	if (offset < 8)
		throw Exception();

	FS_Seek(handle, offset, FS_SEEK_SET);
	return CacheBlock(this, false);
}

// Returns a new block for writing.
CacheBlock CacheStorage::NewBlock ()
{
	FS_Seek(handle, 0, FS_SEEK_END);
	return CacheBlock(this, true);
}

/*
=================
R_LoadMD2Model
=================
*/
int				indRemap[MD2_MAX_TRIANGLES*3];
index_t			tempIndex[MD2_MAX_TRIANGLES*3];
index_t			tempSTIndex[MD2_MAX_TRIANGLES*3];

#include "../shared/MD5.h"

bool R_LoadMD2Model(refModel_t *model)
{
	int				i, j, k;
	int				version, frameSize;
	int				skinWidth, skinHeight;
	int				numVerts, numIndexes;
	double			isw, ish;
	dMd2Coord_t		*inCoord;
	dMd2Frame_t		*inFrame;
	dMd2Header_t	*inModel;
	dMd2Triangle_t	*inTri;
	vec2_t			*outCoord;
	mAliasFrame_t	*outFrame;
	index_t			*outIndex;
	mAliasMesh_t	*outMesh;
	mAliasModel_t	*outModel;
	mAliasSkin_t	*outSkins;
	mAliasVertex_t	*outVertex;
	char			*temp;
	byte			*allocBuffer;
	byte			*buffer;
	int				fileLen;

	// Load the file
	fileLen = FS_LoadFile (model->name, (void **)&buffer, false);
	if (!buffer || fileLen <= 0)
		return false;

	// Check the header
	if (strncmp ((const char *)buffer, MD2_HEADERSTR, 4))
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: '%s' has invalid header\n", model->name);
		return false;
	}

	// Check the version
	inModel = (dMd2Header_t *)buffer;
	version = LittleLong (inModel->version);
	if (version != MD2_MODEL_VERSION)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: '%s' has wrong version number (%i != %i)\n", model->name, version, MD2_MODEL_VERSION);
		return false;
	}

	allocBuffer = (byte*)R_ModAlloc(model, sizeof(mAliasModel_t) + sizeof(mAliasMesh_t));

	model->modelData = outModel = (mAliasModel_t *)allocBuffer;
	model->type = MODEL_MD2;

	//
	// Load the mesh
	//
	allocBuffer += sizeof(mAliasModel_t);
	outMesh = outModel->meshes = (mAliasMesh_t *)allocBuffer;
	outModel->numMeshes = 1;

	Q_strncpyz (outMesh->name, "default", sizeof(outMesh->name));

	outMesh->numVerts = LittleLong (inModel->numVerts);
	if (outMesh->numVerts <= 0 || outMesh->numVerts > MD2_MAX_VERTS)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' has an invalid amount of vertices: %i\n", model->name, outMesh->numVerts);
		return false;
	}

	outMesh->numTris = LittleLong (inModel->numTris);
	if (outMesh->numTris <= 0 || outMesh->numTris > MD2_MAX_TRIANGLES)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' has an invalid amount of triangles: %i\n", model->name, outMesh->numTris);
		return false;
	}

	frameSize = LittleLong (inModel->frameSize);
	outModel->numFrames = LittleLong (inModel->numFrames);
	if (outModel->numFrames <= 0 || outModel->numFrames > MD2_MAX_FRAMES)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' has an invalid amount of frames: %i\n", model->name, outModel->numFrames);
		return false;
	}

	//
	// Load the skins
	//
	skinWidth = LittleLong (inModel->skinWidth);
	skinHeight = LittleLong (inModel->skinHeight);
	if (skinWidth <= 0 || skinHeight <= 0)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' has invalid skin dimensions: %ix%i\n", model->name, skinWidth, skinHeight);
		return false;
	}

	outMesh->numSkins = LittleLong (inModel->numSkins);
	if (outMesh->numSkins < 0 || outMesh->numSkins > MD2_MAX_SKINS)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' has an invalid amount of skins: %i\n", model->name, outMesh->numSkins);
		return false;
	}

	isw = 1.0 / (double)skinWidth;
	ish = 1.0 / (double)skinHeight;

	//
	// No tags
	//
	outModel->numTags = 0;
	outModel->tags = NULL;

	//
	// Load the indexes
	//
	numIndexes = outMesh->numTris * 3;
	outIndex = outMesh->indexes = (index_t*)R_ModAlloc(model, sizeof(index_t) * numIndexes);

	//
	// Load triangle lists
	//
	inTri = (dMd2Triangle_t *) ((byte *)inModel + LittleLong (inModel->ofsTris));
	inCoord = (dMd2Coord_t *) ((byte *)inModel + LittleLong (inModel->ofsST));

	CacheStorage cache ("_pglCache.pca");

	var blockName = (String(model->name) + "__remap");
	var blockMd5 = MD5::GenerateMD5(buffer, fileLen);

	bool isNewBlock;
	CacheBlock &cacheBlock = cache.FindOrCreateBlock(blockName.CString(), isNewBlock);

	if (!isNewBlock)
	{
		byte md5Value[16];
		cacheBlock.ReadBuffer(&md5Value, 16);

		bool isDifferent = false;

		for (int i = 0; i < 16; ++i)
		{
			if (md5Value[i] != blockMd5.ByteValues[i])
			{
				isDifferent = true;
				break;
			}
		}

		if (isDifferent)
		{
			cache.DeleteBlock(cacheBlock);
			isNewBlock = true;
			cacheBlock = cache.CreateBlock(blockName.CString());
		}
	}

	if (isNewBlock)
		cacheBlock.WriteBuffer(blockMd5.ByteValues, 16);

	for (i=0, k=0 ; i <outMesh->numTris; i++, k+=3)
	{
		tempIndex[k+0] = (index_t)LittleShort (inTri[i].vertsIndex[0]);
		tempIndex[k+1] = (index_t)LittleShort (inTri[i].vertsIndex[1]);
		tempIndex[k+2] = (index_t)LittleShort (inTri[i].vertsIndex[2]);

		tempSTIndex[k+0] = (index_t)LittleShort (inTri[i].stIndex[0]);
		tempSTIndex[k+1] = (index_t)LittleShort (inTri[i].stIndex[1]);
		tempSTIndex[k+2] = (index_t)LittleShort (inTri[i].stIndex[2]);
	}

	if (isNewBlock)
	{
		//
		// Build list of unique vertexes
		//
		numVerts = 0;
		for (i=0 ; i<numIndexes ; i++)
			indRemap[i] = -1;

		for (i=0 ; i<numIndexes ; i++)
		{
			if (indRemap[i] != -1)
				continue;

			// Remap duplicates
			for (j=i+1 ; j<numIndexes ; j++)
			{
				if (tempIndex[j] != tempIndex[i])
					continue;
				if (inCoord[tempSTIndex[j]].s != inCoord[tempSTIndex[i]].s
				|| inCoord[tempSTIndex[j]].t != inCoord[tempSTIndex[i]].t)
					continue;

				indRemap[j] = i;
				outIndex[j] = numVerts;
			}

			// Add unique vertex
			indRemap[i] = i;
			outIndex[i] = numVerts++;
		}

		//
		// Remap remaining indexes
		//
		for (i=0 ; i<numIndexes; i++)
		{
			if (indRemap[i] == i)
				continue;

			outIndex[i] = outIndex[indRemap[i]];
		}

		cacheBlock.Write<uint32>(numVerts);
		cacheBlock.WriteBuffer(indRemap, sizeof(int) * numIndexes);
		cacheBlock.WriteBuffer(outIndex, sizeof(int) * numIndexes);
	}
	else
	{
		numVerts = cacheBlock.ReadSimpleType<uint32>();
		cacheBlock.ReadBuffer(indRemap, sizeof(int) * numIndexes);
		cacheBlock.ReadBuffer(outIndex, sizeof(int) * numIndexes);
	}

	if (RB_InvalidMesh(numVerts, numIndexes))
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' mesh is invalid: %i verts, %i indexes\n", model->name, numVerts, numIndexes);
		return false;
	}

	Com_DevPrintf (0, "R_LoadMD2Model: '%s' remapped %i verts to %i (%i tris)\n",
							model->name, outMesh->numVerts, numVerts, outMesh->numTris);
	outMesh->numVerts = numVerts;

	//
	// Load base s and t vertices
	//
	allocBuffer = (byte*)R_ModAlloc(model, (sizeof(vec2_t) * numVerts)
									+ (sizeof(mAliasFrame_t) * outModel->numFrames)
									+ (sizeof(mAliasVertex_t) * outModel->numFrames * numVerts)
									+ (sizeof(vec3_t) * outModel->numFrames * 2)
									+ (sizeof(float) * outModel->numFrames)
									+ (sizeof(mAliasSkin_t) * outMesh->numSkins));

	outCoord = outMesh->coords = (vec2_t *)allocBuffer;

	for (j=0 ; j<numIndexes ; j++)
	{
		outCoord[outIndex[j]][0] = (float)(((double)LittleShort (inCoord[tempSTIndex[indRemap[j]]].s) + 0.5) * isw);
		outCoord[outIndex[j]][1] = (float)(((double)LittleShort (inCoord[tempSTIndex[indRemap[j]]].t) + 0.5) * ish);
	}

	//
	// Load the frames
	//
	allocBuffer += sizeof(vec2_t) * numVerts;
	outFrame = outModel->frames = (mAliasFrame_t *)allocBuffer;

	allocBuffer += sizeof(mAliasFrame_t) * outModel->numFrames;
	outVertex = outMesh->vertexes = (mAliasVertex_t *)allocBuffer;

	allocBuffer += sizeof(mAliasVertex_t) * outModel->numFrames * numVerts;
	outMesh->mins = (vec3_t *)allocBuffer;
	allocBuffer += sizeof(vec3_t) * outModel->numFrames;
	outMesh->maxs = (vec3_t *)allocBuffer;
	allocBuffer += sizeof(vec3_t) * outModel->numFrames;
	outMesh->radius = (float *)allocBuffer;

	for (i=0 ; i<outModel->numFrames; i++, outFrame++, outVertex += numVerts)
	{
		inFrame = (dMd2Frame_t *) ((byte *)inModel + LittleLong (inModel->ofsFrames) + i * frameSize);

		outFrame->scale[0] = LittleFloat (inFrame->scale[0]);
		outFrame->scale[1] = LittleFloat (inFrame->scale[1]);
		outFrame->scale[2] = LittleFloat (inFrame->scale[2]);

		outFrame->translate[0] = LittleFloat (inFrame->translate[0]);
		outFrame->translate[1] = LittleFloat (inFrame->translate[1]);
		outFrame->translate[2] = LittleFloat (inFrame->translate[2]);

		// Frame bounds
		Vec3Copy (outFrame->translate, outFrame->mins);
		Vec3MA (outFrame->translate, 255, outFrame->scale, outFrame->maxs);
		outFrame->radius = RadiusFromBounds (outFrame->mins, outFrame->maxs);

		// Mesh bounds
		Vec3Copy (outFrame->mins, outMesh->mins[i]);
		Vec3Copy (outFrame->maxs, outMesh->maxs[i]);
		outMesh->radius[i] = outFrame->radius;

		// Model bounds
		model->radius = max (model->radius, outFrame->radius);
		AddPointToBounds (outFrame->mins, model->mins, model->maxs);
		AddPointToBounds (outFrame->maxs, model->mins, model->maxs);

		// Load vertices
		for (j=0 ; j<numIndexes ; j++)
		{
			outVertex[outIndex[j]].point[0] = (sint16)inFrame->verts[tempIndex[indRemap[j]]].v[0];
			outVertex[outIndex[j]].point[1] = (sint16)inFrame->verts[tempIndex[indRemap[j]]].v[1];
			outVertex[outIndex[j]].point[2] = (sint16)inFrame->verts[tempIndex[indRemap[j]]].v[2];
		}
		
		if (isNewBlock)
		{
			// Calculate normals
			R_CalcAliasNormals(numIndexes, outIndex, numVerts, outVertex);
		
			for (int x=0 ; x<numVerts ; x++)
				cacheBlock.Write<short>(*(short *)outVertex[x].latLong);	
		}
		else
		{
			for (int x=0 ; x<numVerts ; x++)
				*(short *)outVertex[x].latLong = cacheBlock.ReadSimpleType<short>();	
		}
	}

	//
	// Register all skins
	//
	allocBuffer += sizeof(float) * outModel->numFrames;
	outSkins = outMesh->skins = (mAliasSkin_t *)allocBuffer;

	for (i=0 ; i<outMesh->numSkins ; i++, outSkins++)
	{
		if (LittleLong(inModel->ofsSkins) == -1)
			continue;

		temp = (char *)inModel + LittleLong(inModel->ofsSkins) + i*MD2_MAX_SKINNAME;
		if (!temp || !temp[0])
			continue;

		Com_NormalizePath(outSkins->name, sizeof(outSkins->name), temp);
		outSkins->material = R_RegisterSkin(outSkins->name);
		if (!outSkins->material)
			Com_DevPrintf(PRNT_WARNING, "R_LoadMD2Model: '%s' could not load skin '%s'\n", model->name, outSkins->name);
	}

	if (isNewBlock)
		cacheBlock.Flush();
	cache.Close();

	// Done
	FS_FreeFile (buffer);
	return true;
}

bool R_LoadMD2EModel(refModel_t *model)
{
	int				i, j, k;
	int				version, frameSize;
	int				skinWidth, skinHeight;
	int				numVerts, numIndexes;
	double			isw, ish;
	dMd2eCoord_t	*inCoord;
	dMd2eFrame_t	*inFrame;
	dMd2eHeader_t	*inModel;
	dMd2eTriangle_t	*inTri;
	vec2_t			*outCoord;
	mAliasFrame_t	*outFrame;
	index_t			*outIndex;
	mAliasMesh_t	*outMesh;
	mAliasModel_t	*outModel;
	mAliasSkin_t	*outSkins;
	mAliasVertex_t	*outVertex;
	char			*temp;
	byte			*allocBuffer;
	byte			*buffer;
	int				fileLen;

	// Load the file
	fileLen = FS_LoadFile (model->name, (void **)&buffer, false);
	if (!buffer || fileLen <= 0)
		return false;

	// Check the header
	if (strncmp ((const char *)buffer, MD2E_HEADERSTR, 4))
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: '%s' has invalid header\n", model->name);
		return false;
	}

	// Check the version
	inModel = (dMd2eHeader_t *)buffer;
	version = LittleLong (inModel->version);
	if (version != MD2E_MODEL_VERSION)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: '%s' has wrong version number (%i != %i)\n", model->name, version, MD2E_MODEL_VERSION);
		return false;
	}

	allocBuffer = (byte*)R_ModAlloc(model, sizeof(mAliasModel_t) + sizeof(mAliasMesh_t));

	model->modelData = outModel = (mAliasModel_t *)allocBuffer;
	model->type = MODEL_MD2;

	//
	// Load the mesh
	//
	allocBuffer += sizeof(mAliasModel_t);
	outMesh = outModel->meshes = (mAliasMesh_t *)allocBuffer;
	outModel->numMeshes = 1;

	Q_strncpyz (outMesh->name, "default", sizeof(outMesh->name));

	outMesh->numVerts = LittleLong (inModel->numVerts);
	if (outMesh->numVerts <= 0 || outMesh->numVerts > MD2_MAX_VERTS)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' has an invalid amount of vertices: %i\n", model->name, outMesh->numVerts);
		return false;
	}

	outMesh->numTris = LittleLong (inModel->numTris);
	if (outMesh->numTris <= 0 || outMesh->numTris > MD2_MAX_TRIANGLES)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' has an invalid amount of triangles: %i\n", model->name, outMesh->numTris);
		return false;
	}

	frameSize = LittleLong (inModel->frameSize);
	outModel->numFrames = LittleLong (inModel->numFrames);
	if (outModel->numFrames <= 0 || outModel->numFrames > MD2_MAX_FRAMES)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' has an invalid amount of frames: %i\n", model->name, outModel->numFrames);
		return false;
	}

	//
	// Load the skins
	//
	skinWidth = LittleLong (inModel->skinWidth);
	skinHeight = LittleLong (inModel->skinHeight);
	if (skinWidth <= 0 || skinHeight <= 0)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' has invalid skin dimensions: %ix%i\n", model->name, skinWidth, skinHeight);
		return false;
	}

	outMesh->numSkins = LittleLong (inModel->numSkins);
	if (outMesh->numSkins < 0 || outMesh->numSkins > MD2_MAX_SKINS)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' has an invalid amount of skins: %i\n", model->name, outMesh->numSkins);
		return false;
	}

	isw = 1.0 / (double)skinWidth;
	ish = 1.0 / (double)skinHeight;

	//
	// No tags
	//
	outModel->numTags = 0;
	outModel->tags = NULL;

	//
	// Load the indexes
	//
	numIndexes = outMesh->numTris * 3;
	outIndex = outMesh->indexes = (index_t*)R_ModAlloc(model, sizeof(index_t) * numIndexes);

	//
	// Load triangle lists
	//
	inTri = (dMd2eTriangle_t *) ((byte *)inModel + LittleLong (inModel->ofsTris));
	inCoord = (dMd2eCoord_t *) ((byte *)inModel + LittleLong (inModel->ofsST));

	for (i=0, k=0 ; i <outMesh->numTris; i++, k+=3)
	{
		tempIndex[k+0] = (index_t)LittleShort (inTri[i].vertsIndex[0]);
		tempIndex[k+1] = (index_t)LittleShort (inTri[i].vertsIndex[1]);
		tempIndex[k+2] = (index_t)LittleShort (inTri[i].vertsIndex[2]);

		tempSTIndex[k+0] = (index_t)LittleShort (inTri[i].stIndex[0]);
		tempSTIndex[k+1] = (index_t)LittleShort (inTri[i].stIndex[1]);
		tempSTIndex[k+2] = (index_t)LittleShort (inTri[i].stIndex[2]);
	}

	//
	// Build list of unique vertexes
	//
	numVerts = 0;
	for (i=0 ; i<numIndexes ; i++)
		indRemap[i] = -1;

	for (i=0 ; i<numIndexes ; i++)
	{
		if (indRemap[i] != -1)
			continue;

		// Remap duplicates
		for (j=i+1 ; j<numIndexes ; j++)
		{
			if (tempIndex[j] != tempIndex[i])
				continue;
			if (inCoord[tempSTIndex[j]].s != inCoord[tempSTIndex[i]].s
				|| inCoord[tempSTIndex[j]].t != inCoord[tempSTIndex[i]].t)
				continue;

			indRemap[j] = i;
			outIndex[j] = numVerts;
		}

		// Add unique vertex
		indRemap[i] = i;
		outIndex[i] = numVerts++;
	}

	if (RB_InvalidMesh(numVerts, numIndexes))
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD2Model: model '%s' mesh is invalid: %i verts, %i indexes\n", model->name, numVerts, numIndexes);
		return false;
	}

	Com_DevPrintf (0, "R_LoadMD2Model: '%s' remapped %i verts to %i (%i tris)\n",
		model->name, outMesh->numVerts, numVerts, outMesh->numTris);
	outMesh->numVerts = numVerts;

	//
	// Remap remaining indexes
	//
	for (i=0 ; i<numIndexes; i++)
	{
		if (indRemap[i] == i)
			continue;

		outIndex[i] = outIndex[indRemap[i]];
	}

	//
	// Load base s and t vertices
	//
	allocBuffer = (byte*)R_ModAlloc(model, (sizeof(vec2_t) * numVerts)
		+ (sizeof(mAliasFrame_t) * outModel->numFrames)
		+ (sizeof(mAliasVertex_t) * outModel->numFrames * numVerts)
		+ (sizeof(vec3_t) * outModel->numFrames * 2)
		+ (sizeof(float) * outModel->numFrames)
		+ (sizeof(mAliasSkin_t) * outMesh->numSkins));

	outCoord = outMesh->coords = (vec2_t *)allocBuffer;

	for (j=0 ; j<numIndexes ; j++)
	{
		outCoord[outIndex[j]][0] = inCoord[tempSTIndex[indRemap[j]]].s;
		outCoord[outIndex[j]][1] = inCoord[tempSTIndex[indRemap[j]]].t;
	}

	//
	// Load the frames
	//
	allocBuffer += sizeof(vec2_t) * numVerts;
	outFrame = outModel->frames = (mAliasFrame_t *)allocBuffer;

	allocBuffer += sizeof(mAliasFrame_t) * outModel->numFrames;
	outVertex = outMesh->vertexes = (mAliasVertex_t *)allocBuffer;

	allocBuffer += sizeof(mAliasVertex_t) * outModel->numFrames * numVerts;
	outMesh->mins = (vec3_t *)allocBuffer;
	allocBuffer += sizeof(vec3_t) * outModel->numFrames;
	outMesh->maxs = (vec3_t *)allocBuffer;
	allocBuffer += sizeof(vec3_t) * outModel->numFrames;
	outMesh->radius = (float *)allocBuffer;

	for (i=0 ; i<outModel->numFrames; i++, outFrame++, outVertex += numVerts)
	{
		inFrame = (dMd2eFrame_t *) ((byte *)inModel + LittleLong (inModel->ofsFrames) + i * frameSize);

		outFrame->scale[0] = MD3_XYZ_SCALE;
		outFrame->scale[1] = MD3_XYZ_SCALE;
		outFrame->scale[2] = MD3_XYZ_SCALE;

		outFrame->translate[0] = LittleFloat (inFrame->translate[0]);
		outFrame->translate[1] = LittleFloat (inFrame->translate[1]);
		outFrame->translate[2] = LittleFloat (inFrame->translate[2]);

		// Frame bounds
		Vec3Copy (outFrame->translate, outFrame->mins);
		Vec3MA (outFrame->translate, 255, outFrame->scale, outFrame->maxs);
		outFrame->radius = RadiusFromBounds (outFrame->mins, outFrame->maxs);

		// Mesh bounds
		Vec3Copy (outFrame->mins, outMesh->mins[i]);
		Vec3Copy (outFrame->maxs, outMesh->maxs[i]);
		outMesh->radius[i] = outFrame->radius;

		// Model bounds
		model->radius = max (model->radius, outFrame->radius);
		AddPointToBounds (outFrame->mins, model->mins, model->maxs);
		AddPointToBounds (outFrame->maxs, model->mins, model->maxs);

		// Load vertices
		for (j=0 ; j<numIndexes ; j++)
		{
			outVertex[outIndex[j]].point[0] = (sint16)inFrame->verts[tempIndex[indRemap[j]]].v[0];
			outVertex[outIndex[j]].point[1] = (sint16)inFrame->verts[tempIndex[indRemap[j]]].v[1];
			outVertex[outIndex[j]].point[2] = (sint16)inFrame->verts[tempIndex[indRemap[j]]].v[2];
		}
	}

	//
	// Register all skins
	//
	allocBuffer += sizeof(float) * outModel->numFrames;
	outSkins = outMesh->skins = (mAliasSkin_t *)allocBuffer;

	for (i=0 ; i<outMesh->numSkins ; i++, outSkins++)
	{
		if (LittleLong(inModel->ofsSkins) == -1)
			continue;

		temp = (char *)inModel + LittleLong(inModel->ofsSkins) + i*MD2_MAX_SKINNAME;
		if (!temp || !temp[0])
			continue;

		Com_NormalizePath(outSkins->name, sizeof(outSkins->name), temp);
		outSkins->material = R_RegisterSkin(outSkins->name);
		if (!outSkins->material)
			Com_DevPrintf(PRNT_WARNING, "R_LoadMD2Model: '%s' could not load skin '%s'\n", model->name, outSkins->name);
	}

	// Done
	FS_FreeFile (buffer);
	return true;
}

/*
===============================================================================

	MD3 LOADING

===============================================================================
*/

/*
=================
R_StripModelLODSuffix
=================
*/
static void R_StripModelLODSuffix(char *name)
{
	const int len = strlen(name);
	if (len <= 2)
		return;

	const int lodNum = atoi(&name[len-1]);
	if (lodNum < ALIAS_MAX_LODS)
	{
		if (name[len-2] == '_')
		{
			name[len-2] = '\0';
		}
	}
}


/*
=================
R_LoadMD3Model
=================
*/
bool R_LoadMD3Model(refModel_t *model)
{
	dMd3Coord_t			*inCoord;
	dMd3Frame_t			*inFrame;
	index_t				*inIndex;
	dMd3Header_t		*inModel;
	dMd3Mesh_t			*inMesh;
	dMd3Skin_t			*inSkin;
	dMd3Tag_t			*inTag;
	dMd3Vertex_t		*inVert;
	vec2_t				*outCoord;
	mAliasFrame_t		*outFrame;
	index_t				*outIndex;
	mAliasMesh_t		*outMesh;
	mAliasModel_t		*outModel;
	mAliasSkin_t		*outSkin;
	mAliasTag_t			*outTag;
	mAliasVertex_t		*outVert;
	int					i, j, l;
	int					version;
	byte				*allocBuffer;
	byte				*buffer;
	int					fileLen;

	// Load the file
	fileLen = FS_LoadFile (model->name, (void **)&buffer, false);
	if (!buffer || fileLen <= 0)
		return false;

	// Check the header
	if (strncmp ((const char *)buffer, MD3_HEADERSTR, 4))
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD3Model: '%s' has invalid header\n", model->name);
		return false;
	}

	// Check the version
	inModel = (dMd3Header_t *)buffer;
	version = LittleLong (inModel->version);
	if (version != MD3_MODEL_VERSION)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD3Model: model '%s' has wrong version number (%i != %i)\n", model->name, version, MD3_MODEL_VERSION);
		return false;
	}

	model->modelData = outModel = (mAliasModel_t*)R_ModAlloc(model, sizeof(mAliasModel_t));
	model->type = MODEL_MD3;

	//
	// Byte swap the header fields and sanity check
	//
	outModel->numFrames = LittleLong (inModel->numFrames);
	if (outModel->numFrames <= 0 || outModel->numFrames > MD3_MAX_FRAMES)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD3Model: model '%s' has an invalid amount of frames: %i\n", model->name, outModel->numFrames);
		return false;
	}

	outModel->numTags = LittleLong (inModel->numTags);
	if (outModel->numTags < 0 || outModel->numTags > MD3_MAX_TAGS)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD3Model: model '%s' has an invalid amount of tags: %i\n", model->name, outModel->numTags);
		return false;
	}

	outModel->numMeshes = LittleLong (inModel->numMeshes);
	if (outModel->numMeshes < 0 || outModel->numMeshes > MD3_MAX_MESHES)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD3Model: model '%s' has an invalid amount of meshes: %i\n", model->name, outModel->numMeshes);
		return false;
	}

	if (!outModel->numMeshes && !outModel->numTags)
	{
		FS_FreeFile (buffer);
		Com_Printf (PRNT_ERROR, "R_LoadMD3Model: model '%s' has no meshes and no tags!\n", model->name);
		return false;
	}

	// Allocate as much as possible now
	allocBuffer = (byte*)R_ModAlloc(model, (sizeof(mAliasFrame_t) * outModel->numFrames)
									+ (sizeof(mAliasTag_t) * outModel->numFrames * outModel->numTags)
									+ (sizeof(mAliasMesh_t) * outModel->numMeshes));

	//
	// Load the frames
	//
	inFrame = (dMd3Frame_t *)((byte *)inModel + LittleLong (inModel->ofsFrames));
	outFrame = outModel->frames = (mAliasFrame_t *)allocBuffer;

	for (i=0 ; i<outModel->numFrames ; i++, inFrame++, outFrame++)
	{
		outFrame->scale[0] = MD3_XYZ_SCALE;
		outFrame->scale[1] = MD3_XYZ_SCALE;
		outFrame->scale[2] = MD3_XYZ_SCALE;

		outFrame->translate[0] = LittleFloat (inFrame->translate[0]);
		outFrame->translate[1] = LittleFloat (inFrame->translate[1]);
		outFrame->translate[2] = LittleFloat (inFrame->translate[2]);

		// Never trust the modeler utility and recalculate bbox and radius
		ClearBounds (outFrame->mins, outFrame->maxs);
	}

	//
	// Load the tags
	//
	allocBuffer += sizeof(mAliasFrame_t) * outModel->numFrames;
	inTag = (dMd3Tag_t *)((byte *)inModel + LittleLong (inModel->ofsTags));
	outTag = outModel->tags = (mAliasTag_t *)allocBuffer;

	for (i=0 ; i<outModel->numFrames ; i++)
	{
		for (l=0 ; l<outModel->numTags ; l++, inTag++, outTag++)
		{
			for (j=0 ; j<3 ; j++)
			{
				mat3x3_t	axis;

				axis[0][j] = LittleFloat (inTag->axis[0][j]);
				axis[1][j] = LittleFloat (inTag->axis[1][j]);
				axis[2][j] = LittleFloat (inTag->axis[2][j]);
				Matrix3_Quat (axis, outTag->quat);
				Quat_Normalize (outTag->quat);
				outTag->origin[j] = LittleFloat (inTag->origin[j]);
			}

			Q_strncpyz (outTag->name, inTag->tagName, sizeof(outTag->name));
		}
	}

	//
	// Load the meshes
	//
	allocBuffer += sizeof(mAliasTag_t) * outModel->numFrames * outModel->numTags;
	inMesh = (dMd3Mesh_t *)((byte *)inModel + LittleLong (inModel->ofsMeshes));
	outMesh = outModel->meshes = (mAliasMesh_t *)allocBuffer;

	for (i=0 ; i<outModel->numMeshes ; i++, outMesh++)
	{
		Q_strncpyz (outMesh->name, inMesh->meshName, sizeof(outMesh->name));
		if (strncmp ((const char *)inMesh->ident, MD3_HEADERSTR, 4))
		{
			FS_FreeFile (buffer);
			Com_Printf (PRNT_ERROR, "R_LoadMD3Model: mesh '%s' in model '%s' has wrong id (%i != %i)\n", inMesh->meshName, model->name, LittleLong ((int)inMesh->ident), MD3_HEADER);
			return false;
		}

		R_StripModelLODSuffix(outMesh->name);

		outMesh->numSkins = LittleLong (inMesh->numSkins);
		if (outMesh->numSkins <= 0 || outMesh->numSkins > MD3_MAX_MATERIALS)
		{
			FS_FreeFile (buffer);
			Com_Printf (PRNT_ERROR, "R_LoadMD3Model: mesh '%s' in model '%s' has an invalid amount of skins: %i\n", outMesh->name, model->name, outMesh->numSkins);
			return false;
		}

		outMesh->numTris = LittleLong (inMesh->numTris);
		if (outMesh->numTris <= 0 || outMesh->numTris > MD3_MAX_TRIANGLES)
		{
			FS_FreeFile (buffer);
			Com_Printf (PRNT_ERROR, "R_LoadMD3Model: mesh '%s' in model '%s' has an invalid amount of triangles: %i\n", outMesh->name, model->name, outMesh->numTris);
			return false;
		}

		outMesh->numVerts = LittleLong (inMesh->numVerts);
		if (outMesh->numVerts <= 0 || outMesh->numVerts > MD3_MAX_VERTS)
		{
			FS_FreeFile (buffer);
			Com_Printf (PRNT_ERROR, "R_LoadMD3Model: mesh '%s' in model '%s' has an invalid amount of vertices: %i\n", outMesh->name, model->name, outMesh->numVerts);
			return false;
		}

		if (RB_InvalidMesh(outMesh->numVerts, outMesh->numTris * 3))
		{
			FS_FreeFile (buffer);
			Com_Printf (PRNT_ERROR, "R_LoadMD3Model: mesh '%s' in model '%s' mesh is invalid: %i verts, %i indexes\n", outMesh->name, model->name, outMesh->numVerts, outMesh->numTris * 3);
			return false;
		}

		// Allocate as much as possible now
		allocBuffer = (byte*)R_ModAlloc(model, (sizeof(mAliasSkin_t) * outMesh->numSkins)
										+ (sizeof(index_t) * outMesh->numTris * 3)
										+ (sizeof(vec2_t) * outMesh->numVerts)
										+ (sizeof(mAliasVertex_t) * outModel->numFrames * outMesh->numVerts)
										+ (sizeof(vec3_t) * outModel->numFrames * 2)
										+ (sizeof(float) * outModel->numFrames));

		//
		// Load the skins
		//
		inSkin = (dMd3Skin_t *)((byte *)inMesh + LittleLong (inMesh->ofsSkins));
		outSkin = outMesh->skins = (mAliasSkin_t *)allocBuffer;

		for (j=0 ; j<outMesh->numSkins ; j++, inSkin++, outSkin++)
		{
			if (!inSkin->name || !inSkin->name[0])
				continue;

			Com_NormalizePath(outSkin->name, sizeof(outSkin->name), inSkin->name);
			outSkin->material = R_RegisterSkin(outSkin->name);
			if (!outSkin->material)
				Com_DevPrintf(PRNT_WARNING, "R_LoadMD3Model: '%s' could not load skin '%s' on mesh '%s'\n",
								model->name, outSkin->name, outMesh->name);
		}

		//
		// Load the indexes
		//
		allocBuffer += sizeof(mAliasSkin_t) * outMesh->numSkins;
		inIndex = (index_t *)((byte *)inMesh + LittleLong (inMesh->ofsIndexes));
		outIndex = outMesh->indexes = (index_t *)allocBuffer;

		for (j=0 ; j<outMesh->numTris ; j++, inIndex += 3, outIndex += 3)
		{
			outIndex[0] = (index_t)LittleLong (inIndex[0]);
			outIndex[1] = (index_t)LittleLong (inIndex[1]);
			outIndex[2] = (index_t)LittleLong (inIndex[2]);
		}

		//
		// Load the texture coordinates
		//
		allocBuffer += sizeof(index_t) * outMesh->numTris * 3;
		inCoord = (dMd3Coord_t *)((byte *)inMesh + LittleLong (inMesh->ofsTCs));
		outCoord = outMesh->coords = (vec2_t *)allocBuffer;

		for (j=0 ; j<outMesh->numVerts ; j++, inCoord++)
		{
			outCoord[j][0] = LittleFloat (inCoord->st[0]);
			outCoord[j][1] = LittleFloat (inCoord->st[1]);
		}

		//
		// Load the vertexes and normals
		// Apply vertexes to mesh/model per-frame bounds/radius
		//
		allocBuffer += sizeof(vec2_t) * outMesh->numVerts;
		inVert = (dMd3Vertex_t *)((byte *)inMesh + LittleLong (inMesh->ofsVerts));
		outVert = outMesh->vertexes = (mAliasVertex_t *)allocBuffer;
		outFrame = outModel->frames;

		allocBuffer += sizeof(mAliasVertex_t) * outModel->numFrames * outMesh->numVerts;
		outMesh->mins = (vec3_t *)allocBuffer;
		allocBuffer += sizeof(vec3_t) * outModel->numFrames;
		outMesh->maxs = (vec3_t *)allocBuffer;
		allocBuffer += sizeof(vec3_t) * outModel->numFrames;
		outMesh->radius = (float *)allocBuffer;

		for (l=0 ; l<outModel->numFrames ; l++, outFrame++)
		{
			vec3_t	v;

			ClearBounds (outMesh->mins[l], outMesh->maxs[l]);

			for (j=0 ; j<outMesh->numVerts ; j++, inVert++, outVert++)
			{
				// Vertex
				outVert->point[0] = LittleShort (inVert->point[0]);
				outVert->point[1] = LittleShort (inVert->point[1]);
				outVert->point[2] = LittleShort (inVert->point[2]);

				// Add vertex to bounds
				Vec3Copy (outVert->point, v);
				AddPointToBounds (v, outFrame->mins, outFrame->maxs);
				AddPointToBounds (v, outMesh->mins[l], outMesh->maxs[l]);

				// Normal
				outVert->latLong[0] = inVert->norm[0] & 0xff;
				outVert->latLong[1] = inVert->norm[1] & 0xff;
			}

			outMesh->radius[l] = RadiusFromBounds (outMesh->mins[l], outMesh->maxs[l]);
		}

		// End of loop
		inMesh = (dMd3Mesh_t *)((byte *)inMesh + LittleLong (inMesh->meshSize));
	}

	//
	// Calculate model bounds
	//
	outFrame = outModel->frames;
	for (i=0 ; i<outModel->numFrames ; i++, outFrame++)
	{
		Vec3MA (outFrame->translate, MD3_XYZ_SCALE, outFrame->mins, outFrame->mins);
		Vec3MA (outFrame->translate, MD3_XYZ_SCALE, outFrame->maxs, outFrame->maxs);
		outFrame->radius = RadiusFromBounds (outFrame->mins, outFrame->maxs);

		AddPointToBounds (outFrame->mins, model->mins, model->maxs);
		AddPointToBounds (outFrame->maxs, model->mins, model->maxs);
		model->radius = max (model->radius, outFrame->radius);
	}

	// Done
	FS_FreeFile (buffer);
	return true;
}
