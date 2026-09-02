/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/VertMaterial.cpp                       $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 8/22/01 11:06a                                              $*
 *                                                                                             *
 *                    $Revision:: 42                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   Init -- init code                                                                         *
 *   Shutdown -- shutdown code                                                                 *
 *   Get_Preset -- retrieve presets                                                            *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "VertMaterial.h"
#include "WWLib/realcrc.h"
#include "WWDebug/wwdebug.h"
#include "W3DUtil.h"
#include "WWLib/chunkio.h"
#include "W3DErr.h"
#include "WWLib/INI.h"
#include "WWLib/XSTRAW.h"
#include "WW3D2/Backend/RenderBackend.h"
#include "WW3D.h"


static unsigned int unique=1;

VertexMaterialClass* VertexMaterialClass::Presets[VertexMaterialClass::PRESET_COUNT];

/*
** VertexMaterialClass Implementation
*/
VertexMaterialClass::VertexMaterialClass():
	Material{},
	Flags(0),
	AmbientColorSource(MATERIAL),
	EmissiveColorSource(MATERIAL),
	DiffuseColorSource(MATERIAL),
	UseLighting(false),
	UniqueID(0),
	CRCDirty(true)
{
	int i;

	for (i=0; i<MeshBuilderClass::MAX_STAGES; i++)
	{
		Mapper[i]=nullptr;
		UVSource[i] = i;
	}
	Set_Ambient(1.0f,1.0f,1.0f);
	Set_Diffuse(1.0f,1.0f,1.0f);

	Set_Opacity(1.0f);
}

VertexMaterialClass::VertexMaterialClass(const VertexMaterialClass & src) :
	Material(src.Material),
	Flags(src.Flags),
	AmbientColorSource(src.AmbientColorSource),
	EmissiveColorSource(src.EmissiveColorSource),
	DiffuseColorSource(src.DiffuseColorSource),
	UseLighting(src.UseLighting),
	Name(src.Name),
	UniqueID(src.UniqueID),
	CRCDirty(true)
{
	int i;
	for (i=0; i<MeshBuilderClass::MAX_STAGES; i++)
	{
		Mapper[i]=nullptr;
		if (src.Mapper[i])
		{
			TextureMapperClass *mapper=src.Mapper[i]->Clone();
			Set_Mapper(mapper,i);
			mapper->Release_Ref();
		}

		UVSource[i] = src.UVSource[i];
	}
}

void VertexMaterialClass::Make_Unique()
{
	CRCDirty=true;
	UniqueID=unique;
	unique++;
}

VertexMaterialClass::~VertexMaterialClass()
{
	int i;

	for (i=0; i<MeshBuilderClass::MAX_STAGES; i++)
	{
		if (Mapper[i])
		{
			REF_PTR_RELEASE(Mapper[i]);
			Mapper[i]=nullptr;
		}
	}
}

VertexMaterialClass & VertexMaterialClass::operator = (const VertexMaterialClass &src)
{

	if (this != &src) {
		Name=src.Name;
		Flags = src.Flags;
		AmbientColorSource = src.AmbientColorSource;
		EmissiveColorSource = src.EmissiveColorSource;
		DiffuseColorSource = src.DiffuseColorSource;
		UseLighting=src.UseLighting;
		UniqueID=src.UniqueID;
		CRCDirty=src.CRCDirty;
		int stage;
		for (stage=0;stage<MeshBuilderClass::MAX_STAGES;++stage) {
			if (Mapper[stage] != nullptr) {
				Mapper[stage]->Release_Ref();
				Mapper[stage] = nullptr;
			}
		}
		for (stage=0;stage<MeshBuilderClass::MAX_STAGES;++stage) {
			if (src.Mapper[stage]) {
				TextureMapperClass *mapper = src.Mapper[stage]->Clone();
				Set_Mapper(mapper,stage);
				mapper->Release_Ref();
			}
			UVSource[stage] = src.UVSource[stage];
		}

		Material = src.Material;
	}
	return *this;
}

unsigned long VertexMaterialClass::Compute_CRC() const
{
	unsigned long crc = 0;

// don't include the name when determining whether two vertex materials match
//	crc = CRC_Memory(reinterpret_cast<const unsigned char *>(Name.Peek_Buffer()),sizeof(char)*strlen(Name),crc);

	crc = CRC_Memory(reinterpret_cast<const unsigned char *>(&Material),sizeof(Material),crc);
	crc = CRC_Memory(reinterpret_cast<const unsigned char *>(&Flags),sizeof(Flags),crc);
	crc = CRC_Memory(reinterpret_cast<const unsigned char *>(&DiffuseColorSource),sizeof(DiffuseColorSource),crc);
	crc = CRC_Memory(reinterpret_cast<const unsigned char *>(&AmbientColorSource),sizeof(AmbientColorSource),crc);
	crc = CRC_Memory(reinterpret_cast<const unsigned char *>(&EmissiveColorSource),sizeof(EmissiveColorSource),crc);
	crc = CRC_Memory(reinterpret_cast<const unsigned char *>(&UVSource),sizeof(UVSource),crc);
	crc = CRC_Memory(reinterpret_cast<const unsigned char *>(&UseLighting),sizeof(UseLighting),crc);
	crc = CRC_Memory(reinterpret_cast<const unsigned char *>(&UniqueID),sizeof(UniqueID),crc);

	int i;
	for (i=0; i<MeshBuilderClass::MAX_STAGES; i++)
	{
		if (Mapper[i]) crc = CRC_Memory(reinterpret_cast<const unsigned char *>(&(Mapper[i])),sizeof(TextureMapperClass*),crc);
	}

	return crc;
}

// Ambient Get and Sets

void VertexMaterialClass::Get_Ambient(Vector3 * set) const
{
	assert(set);
	*set=Vector3(Material.ambient[0],Material.ambient[1],Material.ambient[2]);
}

void VertexMaterialClass::Set_Ambient(const Vector3 & color)
{
	CRCDirty=true;
	Material.ambient[0]=color.X;
	Material.ambient[1]=color.Y;
	Material.ambient[2]=color.Z;
}

void VertexMaterialClass::Set_Ambient(float r,float g,float b)
{
	CRCDirty=true;
	Material.ambient[0]=r;
	Material.ambient[1]=g;
	Material.ambient[2]=b;
}

// Diffuse Get and Sets

void VertexMaterialClass::Get_Diffuse(Vector3 * set) const
{
	assert(set);
	*set=Vector3(Material.diffuse[0],Material.diffuse[1],Material.diffuse[2]);
}

void VertexMaterialClass::Set_Diffuse(const Vector3 & color)
{
	CRCDirty=true;
	Material.diffuse[0]=color.X;
	Material.diffuse[1]=color.Y;
	Material.diffuse[2]=color.Z;
}

void VertexMaterialClass::Set_Diffuse(float r,float g,float b)
{
	CRCDirty=true;
	Material.diffuse[0]=r;
	Material.diffuse[1]=g;
	Material.diffuse[2]=b;
}

// Specular Get and Sets

void VertexMaterialClass::Get_Specular(Vector3 * set) const
{
	assert(set);
	*set=Vector3(Material.specular[0],Material.specular[1],Material.specular[2]);
}

void VertexMaterialClass::Set_Specular(const Vector3 & color)
{
	CRCDirty=true;
	Material.specular[0]=color.X;
	Material.specular[1]=color.Y;
	Material.specular[2]=color.Z;
}

void VertexMaterialClass::Set_Specular(float r,float g,float b)
{
	CRCDirty=true;
	Material.specular[0]=r;
	Material.specular[1]=g;
	Material.specular[2]=b;
}

// Emissive Get and Sets

void VertexMaterialClass::Get_Emissive(Vector3 * set) const
{
	assert(set);
	*set=Vector3(Material.emissive[0],Material.emissive[1],Material.emissive[2]);
}

void VertexMaterialClass::Set_Emissive(const Vector3 & color)
{
	CRCDirty=true;
	Material.emissive[0]=color.X;
	Material.emissive[1]=color.Y;
	Material.emissive[2]=color.Z;
}

void VertexMaterialClass::Set_Emissive(float r,float g,float b)
{
	CRCDirty=true;
	Material.emissive[0]=r;
	Material.emissive[1]=g;
	Material.emissive[2]=b;
}


float	VertexMaterialClass::Get_Shininess() const
{
	return Material.power;
}

void	VertexMaterialClass::Set_Shininess(float shin)
{
	CRCDirty=true;
	Material.power=shin;
}

float	VertexMaterialClass::Get_Opacity() const
{
	return Material.diffuse[3];
}

void	VertexMaterialClass::Set_Opacity(float o)
{
	CRCDirty=true;
	Material.diffuse[3]=o;
}

void	VertexMaterialClass::Set_Ambient_Color_Source(ColorSourceType src)
{
	CRCDirty=true;
	AmbientColorSource = src;
}

void	VertexMaterialClass::Set_Emissive_Color_Source(ColorSourceType src)
{
	CRCDirty=true;
	EmissiveColorSource = src;
}

void	VertexMaterialClass::Set_Diffuse_Color_Source(ColorSourceType src)
{
	CRCDirty=true;
	DiffuseColorSource = src;
}

VertexMaterialClass::ColorSourceType
VertexMaterialClass::Get_Ambient_Color_Source()
{
	return AmbientColorSource;
}

VertexMaterialClass::ColorSourceType
VertexMaterialClass::Get_Emissive_Color_Source()
{
	return EmissiveColorSource;
}

VertexMaterialClass::ColorSourceType
VertexMaterialClass::Get_Diffuse_Color_Source()
{
	return DiffuseColorSource;
}

void VertexMaterialClass::Set_UV_Source(int stage,int array_index)
{
	WWASSERT(stage >= 0);
	WWASSERT(stage < MeshBuilderClass::MAX_STAGES);
	WWASSERT(array_index >= 0);
	WWASSERT(array_index < 8);
	CRCDirty=true;
	UVSource[stage] = array_index;
}

int VertexMaterialClass::Get_UV_Source(int stage)
{
	WWASSERT(stage >= 0);
	WWASSERT(stage < MeshBuilderClass::MAX_STAGES);
	return UVSource[stage];
}


void VertexMaterialClass::Init_From_Material3(const W3dMaterial3Struct & mat3)
{
	Vector3 tmp0,tmp1,tmp2;

	W3dUtilityClass::Convert_Color(mat3.DiffuseColor,&tmp0);
	W3dUtilityClass::Convert_Color(mat3.DiffuseCoefficients,&tmp1);
	tmp2.X = tmp0.X * tmp1.X;
	tmp2.Y = tmp0.Y * tmp1.Y;
	tmp2.Z = tmp0.Z * tmp1.Z;
	Set_Diffuse(tmp2);

	W3dUtilityClass::Convert_Color(mat3.SpecularColor,&tmp0);
	W3dUtilityClass::Convert_Color(mat3.SpecularCoefficients,&tmp1);
	tmp2.X = tmp0.X * tmp1.X;
	tmp2.Y = tmp0.Y * tmp1.Y;
	tmp2.Z = tmp0.Z * tmp1.Z;
	Set_Specular(tmp2);

	W3dUtilityClass::Convert_Color(mat3.EmissiveCoefficients,&tmp0);
	Set_Emissive(tmp0);

	W3dUtilityClass::Convert_Color(mat3.AmbientCoefficients,&tmp0);
	Set_Ambient(tmp0);

	Set_Shininess(mat3.Shininess);
	Set_Opacity(mat3.Opacity);
}

WW3DErrorType VertexMaterialClass::Load_W3D(ChunkLoadClass & cload)
{
	char name[256];

	W3dVertexMaterialStruct vmat;
	bool hasname = false;

	char *mapping0_arg_buffer = nullptr;
	char *mapping1_arg_buffer = nullptr;
	unsigned int mapping0_arg_len = 0U;
	unsigned int mapping1_arg_len = 0U;

	while (cload.Open_Chunk()) {
		switch (cload.Cur_Chunk_ID()) {
			case W3D_CHUNK_VERTEX_MATERIAL_NAME:
				cload.Read(&name,cload.Cur_Chunk_Length());
				hasname = true;
				break;

			case W3D_CHUNK_VERTEX_MATERIAL_INFO:
				if (cload.Read(&vmat,sizeof(vmat)) != sizeof(vmat)) {
					return WW3D_ERROR_LOAD_FAILED;
				}
				break;

			case W3D_CHUNK_VERTEX_MAPPER_ARGS0:
				mapping0_arg_len = cload.Cur_Chunk_Length();
				mapping0_arg_buffer = MSGW3DNEWARRAY("VertexMaterialClassTemp") char[mapping0_arg_len];
				if (cload.Read(mapping0_arg_buffer, mapping0_arg_len) != mapping0_arg_len) {
					return WW3D_ERROR_LOAD_FAILED;
				}
				break;

			case W3D_CHUNK_VERTEX_MAPPER_ARGS1:
				mapping1_arg_len = cload.Cur_Chunk_Length();
				mapping1_arg_buffer = MSGW3DNEWARRAY("VertexMaterialClassTemp") char[mapping1_arg_len];
				if (cload.Read(mapping1_arg_buffer, mapping1_arg_len) != mapping1_arg_len) {
					return WW3D_ERROR_LOAD_FAILED;
				}
				break;
		};
		cload.Close_Chunk();
	}

	if (hasname) {
		Set_Name(name);
	}

	Parse_W3dVertexMaterialStruct(vmat);
	Parse_Mapping_Args(vmat,mapping0_arg_buffer,mapping1_arg_buffer);

	delete [] mapping0_arg_buffer;
	mapping0_arg_buffer = nullptr;

	delete [] mapping1_arg_buffer;
	mapping1_arg_buffer = nullptr;

	return WW3D_ERROR_OK;
}

void VertexMaterialClass::Parse_W3dVertexMaterialStruct(const W3dVertexMaterialStruct & vmat)
{
	Vector3 tmp;
	W3dUtilityClass::Convert_Color(vmat.Ambient,&tmp);
	Set_Ambient(tmp);

	W3dUtilityClass::Convert_Color(vmat.Diffuse,&tmp);
	Set_Diffuse(tmp);

	W3dUtilityClass::Convert_Color(vmat.Specular,&tmp);
	Set_Specular(tmp);

	W3dUtilityClass::Convert_Color(vmat.Emissive,&tmp);
	Set_Emissive(tmp);

	Set_Shininess(vmat.Shininess);
	Set_Opacity(vmat.Opacity);

	if (vmat.Attributes & W3DVERTMAT_USE_DEPTH_CUE) {
		Set_Flag(VertexMaterialClass::DEPTH_CUE,true);
	}

	if (vmat.Attributes & W3DVERTMAT_COPY_SPECULAR_TO_DIFFUSE) {
		Set_Flag(VertexMaterialClass::COPY_SPECULAR_TO_DIFFUSE,true);
	}
}

void VertexMaterialClass::Parse_Mapping_Args(const W3dVertexMaterialStruct & vmat,char * mapping0_arg_buffer,char * mapping1_arg_buffer)
{

	// Read an INIClass from the mapping argument buffer - this will be used
	// to initialize any special mappers used.
	INIClass mapping0_arg_ini;
	if (mapping0_arg_buffer) {

		int mapping0_arg_len = strlen(mapping0_arg_buffer);

		char *extended_arg_buffer = MSGW3DNEWARRAY("VertexMaterialClassTemp") char[mapping0_arg_len + 10];
		snprintf(extended_arg_buffer, mapping0_arg_len + 10, "[Args]\n%s", mapping0_arg_buffer);
		mapping0_arg_len = strlen(extended_arg_buffer) + 1;

		BufferStraw map_arg_buf_straw((void *)extended_arg_buffer, mapping0_arg_len);

		mapping0_arg_ini.Load(map_arg_buf_straw);

		delete [] extended_arg_buffer;
		extended_arg_buffer = nullptr;
	}
	INIClass mapping1_arg_ini;
	if (mapping1_arg_buffer) {

		int mapping1_arg_len = strlen(mapping1_arg_buffer);

		char *extended_arg_buffer = MSGW3DNEWARRAY("VertexMaterialClassTemp") char[mapping1_arg_len + 20];
		snprintf(extended_arg_buffer, mapping1_arg_len + 20, "[Args]\n%s", mapping1_arg_buffer);
		mapping1_arg_len = strlen(extended_arg_buffer) + 1;

		BufferStraw map_arg_buf_straw((void *)extended_arg_buffer, mapping1_arg_len);

		mapping1_arg_ini.Load(map_arg_buf_straw);

		delete [] extended_arg_buffer;
		extended_arg_buffer = nullptr;
	}

	// Set up the vertex mapper.  If it is one of the simple
	// ones, set the pointer to one of the global instances.
	int mapping = vmat.Attributes & W3DVERTMAT_STAGE0_MAPPING_MASK;

	switch(mapping) {

		case W3DVERTMAT_STAGE0_MAPPING_UV:
			break;

		case W3DVERTMAT_STAGE0_MAPPING_ENVIRONMENT:
			{
				EnvironmentMapperClass *mapper = NEW_REF(EnvironmentMapperClass,(0));
				Set_Mapper(mapper);
				mapper->Release_Ref();
			}
			break;
		case W3DVERTMAT_STAGE0_MAPPING_CHEAP_ENVIRONMENT:
			{
				ClassicEnvironmentMapperClass *mapper = NEW_REF(ClassicEnvironmentMapperClass,(0));
				Set_Mapper(mapper);
				mapper->Release_Ref();
			}
			break;
		case W3DVERTMAT_STAGE0_MAPPING_LINEAR_OFFSET:
			{
				LinearOffsetTextureMapperClass *mapper =
					NEW_REF(LinearOffsetTextureMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_SCREEN:
			{
				ScreenMapperClass *mapper =
					NEW_REF(ScreenMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_SCALE:
			{
				ScaleTextureMapperClass *mapper =
					NEW_REF(ScaleTextureMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_GRID:
			{
				GridTextureMapperClass *mapper =
					NEW_REF(GridTextureMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_ROTATE:
			{
				RotateTextureMapperClass *mapper =
					NEW_REF(RotateTextureMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_SINE_LINEAR_OFFSET:
			{
				SineLinearOffsetTextureMapperClass *mapper =
					NEW_REF(SineLinearOffsetTextureMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_STEP_LINEAR_OFFSET:
			{
				StepLinearOffsetTextureMapperClass *mapper =
					NEW_REF(StepLinearOffsetTextureMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_ZIGZAG_LINEAR_OFFSET:
			{
				ZigZagLinearOffsetTextureMapperClass *mapper =
					NEW_REF(ZigZagLinearOffsetTextureMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_WS_CLASSIC_ENV:
			{
				WSClassicEnvironmentMapperClass *mapper = NEW_REF(WSClassicEnvironmentMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_WS_ENVIRONMENT:
			{
				WSEnvironmentMapperClass *mapper = NEW_REF(WSEnvironmentMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_GRID_CLASSIC_ENV:
			{
				GridClassicEnvironmentMapperClass *mapper =
					NEW_REF(GridClassicEnvironmentMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_GRID_ENVIRONMENT:
			{
				GridEnvironmentMapperClass *mapper =
					NEW_REF(GridEnvironmentMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_RANDOM:
			{
				RandomTextureMapperClass *mapper =
					NEW_REF(RandomTextureMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_EDGE:
		{
			EdgeMapperClass *mapper =
				NEW_REF(EdgeMapperClass,(mapping0_arg_ini, "Args", 0));
			Set_Mapper(mapper,0);
			mapper->Release_Ref();
		}
		break;

		case W3DVERTMAT_STAGE0_MAPPING_BUMPENV:
		{
			BumpEnvTextureMapperClass *mapper =
				NEW_REF(BumpEnvTextureMapperClass,(mapping0_arg_ini, "Args", 0));
			Set_Mapper(mapper,0);
			mapper->Release_Ref();
		}
		break;

		case W3DVERTMAT_STAGE0_MAPPING_GRID_WS_CLASSIC_ENV:
			{
				GridWSClassicEnvironmentMapperClass *mapper =
					NEW_REF(GridWSClassicEnvironmentMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE0_MAPPING_GRID_WS_ENVIRONMENT:
			{
				GridWSEnvironmentMapperClass *mapper =
					NEW_REF(GridWSEnvironmentMapperClass,(mapping0_arg_ini, "Args", 0));
				Set_Mapper(mapper,0);
				mapper->Release_Ref();
			}
			break;

		default:
			break;
	}

	// Same setup for stage 1's mapper.
	mapping = vmat.Attributes & W3DVERTMAT_STAGE1_MAPPING_MASK;
	switch(mapping) {

		case W3DVERTMAT_STAGE1_MAPPING_UV:
			break;

		case W3DVERTMAT_STAGE1_MAPPING_ENVIRONMENT:
		{
			EnvironmentMapperClass *mapper = W3DNEW EnvironmentMapperClass(1);
			Set_Mapper(mapper, 1);
			mapper->Release_Ref();
		}
		break;
		case W3DVERTMAT_STAGE1_MAPPING_CHEAP_ENVIRONMENT:
		{
			ClassicEnvironmentMapperClass *mapper = W3DNEW ClassicEnvironmentMapperClass(1);
			Set_Mapper(mapper, 1);
			mapper->Release_Ref();
		}
		break;

		case W3DVERTMAT_STAGE1_MAPPING_LINEAR_OFFSET:
		{
			LinearOffsetTextureMapperClass *mapper =
				W3DNEW LinearOffsetTextureMapperClass(mapping1_arg_ini, "Args", 1);
			Set_Mapper(mapper, 1);
			mapper->Release_Ref();
		}
		break;

		case W3DVERTMAT_STAGE1_MAPPING_SCREEN:
		{
			ScreenMapperClass *mapper =
				W3DNEW ScreenMapperClass(mapping1_arg_ini, "Args", 1);
			Set_Mapper(mapper, 1);
			mapper->Release_Ref();
		}
		break;

		case W3DVERTMAT_STAGE1_MAPPING_SCALE:
			{
				ScaleTextureMapperClass *mapper =
					NEW_REF(ScaleTextureMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_GRID:
			{
				GridTextureMapperClass *mapper =
					NEW_REF(GridTextureMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_ROTATE:
			{
				RotateTextureMapperClass *mapper =
					NEW_REF(RotateTextureMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_SINE_LINEAR_OFFSET:
			{
				SineLinearOffsetTextureMapperClass *mapper =
					NEW_REF(SineLinearOffsetTextureMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_STEP_LINEAR_OFFSET:
			{
				StepLinearOffsetTextureMapperClass *mapper =
					NEW_REF(StepLinearOffsetTextureMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_ZIGZAG_LINEAR_OFFSET:
			{
				ZigZagLinearOffsetTextureMapperClass *mapper =
					NEW_REF(ZigZagLinearOffsetTextureMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_WS_CLASSIC_ENV:
			{
				WSClassicEnvironmentMapperClass *mapper = NEW_REF(WSClassicEnvironmentMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_WS_ENVIRONMENT:
			{
				WSEnvironmentMapperClass *mapper = NEW_REF(WSEnvironmentMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_GRID_CLASSIC_ENV:
			{
				GridClassicEnvironmentMapperClass *mapper =
					NEW_REF(GridClassicEnvironmentMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_GRID_ENVIRONMENT:
			{
				GridEnvironmentMapperClass *mapper =
					NEW_REF(GridEnvironmentMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_RANDOM:
			{
				RandomTextureMapperClass *mapper =
					NEW_REF(RandomTextureMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_EDGE:
			{
				EdgeMapperClass *mapper =
					NEW_REF(EdgeMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_BUMPENV:
			{
				BumpEnvTextureMapperClass *mapper =
					NEW_REF(BumpEnvTextureMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

	case W3DVERTMAT_STAGE1_MAPPING_GRID_WS_CLASSIC_ENV:
			{
				GridWSClassicEnvironmentMapperClass *mapper =
					NEW_REF(GridWSClassicEnvironmentMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		case W3DVERTMAT_STAGE1_MAPPING_GRID_WS_ENVIRONMENT:
			{
				GridWSEnvironmentMapperClass *mapper =
					NEW_REF(GridWSEnvironmentMapperClass,(mapping1_arg_ini, "Args", 1));
				Set_Mapper(mapper,1);
				mapper->Release_Ref();
			}
			break;

		default:
			break;
	}
}


WW3DErrorType VertexMaterialClass::Save_W3D(ChunkSaveClass & csave)
{
	WWASSERT(0);
	return WW3D_ERROR_OK;
}

void VertexMaterialClass::Apply() const
{
	int i;
	const auto to_backend_source = [](ColorSourceType source) {
		switch (source)
		{
			case COLOR1: return RenderBackendMaterialSource::Color1;
			case COLOR2: return RenderBackendMaterialSource::Color2;
			default: return RenderBackendMaterialSource::MaterialValue;
		}
	};

	WW3D::Get_Render_Backend()->Set_Material_Values(Material);

	if (WW3D::Is_Coloring_Enabled())
		WW3D::Get_Render_Backend()->Set_Lighting_Enabled(false);
	else
		WW3D::Get_Render_Backend()->Set_Lighting_Enabled(UseLighting);
	WW3D::Get_Render_Backend()->Set_Material_Color_Sources(
		to_backend_source(AmbientColorSource),
		to_backend_source(DiffuseColorSource),
		to_backend_source(EmissiveColorSource));

	// set to default values if no mappers
	for (i=0; i<MeshBuilderClass::MAX_STAGES; i++) {
		if (Mapper[i]) {
			Mapper[i]->Apply(UVSource[i]);
		} else {
			WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(i,RenderBackendTextureCoordinateSource::PassThrough,UVSource[i]);
			WW3D::Get_Render_Backend()->Set_Texture_Transform_Flags(i,RenderBackendTextureTransformFlags::Disabled);
		}
	}
}

void VertexMaterialClass::Apply_Null()
{
	int i;
	static const RenderBackendMaterial default_settings =
	{
		{ 1.0f, 1.0f, 1.0f, 1.0f },	// diffuse
		{ 1.0f, 1.0f, 1.0f, 1.0f },	// ambient
		{ 0.0f, 0.0f, 0.0f, 0.0f },	// specular
		{ 0.0f, 0.0f, 0.0f, 0.0f },	// emissive
		1.0f									// power
	};

	WW3D::Get_Render_Backend()->Set_Lighting_Enabled(false);
	WW3D::Get_Render_Backend()->Set_Material_Values(default_settings);

	WW3D::Get_Render_Backend()->Set_Material_Color_Sources(
		RenderBackendMaterialSource::MaterialValue,
		RenderBackendMaterialSource::MaterialValue,
		RenderBackendMaterialSource::MaterialValue);

	// set to default values if no mappers
	for (i=0; i<MeshBuilderClass::MAX_STAGES; i++) {
		WW3D::Get_Render_Backend()->Set_Texture_Coordinate_Source(i,RenderBackendTextureCoordinateSource::PassThrough,i);
		WW3D::Get_Render_Backend()->Set_Texture_Transform_Flags(i,RenderBackendTextureTransformFlags::Disabled);
	}
}


/***********************************************************************************************
 * Init -- init code                                                                           *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   2/14/2001  hy : Created.                                                                  *
 *=============================================================================================*/
void VertexMaterialClass::Init()
{
	int i;
	for (i=0; i<PRESET_COUNT;i++)
		Presets[i]=NEW_REF(VertexMaterialClass,());

	// Set up presets
	Presets[PRELIT_DIFFUSE]->Set_Diffuse_Color_Source(VertexMaterialClass::COLOR1);
	Presets[PRELIT_DIFFUSE]->Set_Lighting(false);
	Presets[PRELIT_NODIFFUSE]->Set_Lighting(false);
}


/***********************************************************************************************
 * Shutdown -- shutdown code                                                                   *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   2/14/2001  hy : Created.                                                                  *
 *=============================================================================================*/
void VertexMaterialClass::Shutdown()
{
	int i;
	for (i=0; i<PRESET_COUNT;i++)
		REF_PTR_RELEASE(Presets[i]);
}


/***********************************************************************************************
 * Get_Preset -- retrieve presets                                                              *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   2/14/2001  hy : Created.                                                                  *
 *=============================================================================================*/
VertexMaterialClass * VertexMaterialClass::Get_Preset(PresetType type)
{
	WWASSERT(type<PRESET_COUNT);
	Presets[type]->Add_Ref();
	return Presets[type];
}
