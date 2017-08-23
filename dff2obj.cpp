#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum eRWSectionType
{
	rwDATA				= 0x1,
	rwSTRING			= 0x2,
	rwEXTENSION			= 0x3,
	rwCLUMP				= 0x10,
	rwGEOMETRYLIST		= 0x1A
};

enum eRWSectionGeometryFormat
{
	rpGEOMETRYTRISTRIP				= 0x00000001,
	rpGEOMETRYPOSITIONS				= 0x00000002,
	rpGEOMETRYTEXTURED				= 0x00000004,
	rpGEOMETRYPRELIT				= 0x00000008,
	rpGEOMETRYNORMALS				= 0x00000010,
	rpGEOMETRYLIGHT					= 0x00000020,
	rpGEOMETRYMODULATEMATERIALCOLOR = 0x00000040,
	rpGEOMETRYTEXTURED2				= 0x00000080,
	rpGEOMETRYNATIVE				= 0x01000000,
};

struct Section 
{
	int sectionType;
	int size;
	int version;
};

bool Dff2Obj(const char* infile, const char* outfile)
{
	bool ok = false;
	FILE* out = NULL;

	short* triangles = NULL;
	float** uvs = NULL;
	float* vertices = NULL;
	float* normals = NULL;
	int flag;
	int numTriangles;
	int numVertices;
	int numMorphTargets;
	int numTexSet = 0;

	FILE* in = fopen(infile, "rb");
	if (NULL == in)
	{
		printf("can't open file:%s!", infile);
		goto END;
	}

	Section section;

#define READ_VALUE(name, value, size) if (fread(value, size, 1, in) != 1)\
	{\
		printf("read value %s failed!", name); \
		goto END; \
	}

#define READ_SECTION(name, skip) if (fread(&section, sizeof(section), 1, in) != 1)\
	{\
		printf("can't read section:%s!", name);\
		goto END; \
	}\
	if (skip)\
	{\
		fseek(in, section.size, SEEK_CUR);\
	}

	// begin parse
	READ_SECTION("Clump", false);
	READ_SECTION("Clump Struct", true);
	READ_SECTION("Frame List", true);
	READ_SECTION("Geometry List", false);
	READ_SECTION("Geometry List Struct", true);
	READ_SECTION("Geometry", false);
	READ_SECTION("Geometry Struct", false);

	// geometry
	READ_VALUE("Geometry Struct flag", &flag, 4);
	READ_VALUE("Geometry Struct numTriangles", &numTriangles, 4);
	READ_VALUE("Geometry Struct numVertices", &numVertices, 4);
	READ_VALUE("Geometry Struct numMorphTargets", &numMorphTargets, 4);

	if (numMorphTargets != 1)
	{
		printf("numMorphTargets == %d", numMorphTargets);
		goto END;
	}
	
	if ((flag & rpGEOMETRYNATIVE) == 0)
	{
		// prelitcolor
		if (flag & rpGEOMETRYPRELIT)
		{
			fseek(in, numVertices * 4, SEEK_CUR);
		}

		// uvs
		if (flag & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2))
		{
			uvs = (float**)malloc(sizeof(float*)* 2);
			numTexSet = (flag & 0x00FF0000) >> 16;
			for (int i = 0; i < numTexSet; ++i)
			{
				uvs[i] = (float*)malloc(numVertices * 8);
				READ_VALUE("Geometry Struct uv", uvs[i], numVertices * 8);
			}
		}

		// triangles
		triangles = (short*)malloc(numTriangles * 8);
		READ_VALUE("Geometry Struct triangles", triangles, numTriangles * 8);
	}

	// morph targets
	for (int i = 0; i < numMorphTargets && i < 1; ++i)
	{
		// bounding sphere
		fseek(in, 16, SEEK_CUR);

		int hasVertices = 0;
		int hasNormals = 0;

		READ_VALUE("Geometry Struct hasVertices", &hasVertices, 4);
		READ_VALUE("Geometry Struct hasNormals", &hasNormals, 4);

		// vertices
		if (hasVertices)
		{
			vertices = (float*)malloc(numVertices * 12);
			READ_VALUE("Geometry Struct vertices", vertices, numVertices * 12);
		}

		// normals
		if (hasNormals)
		{
			normals = (float*)malloc(numVertices * 12);
			READ_VALUE("Geometry Struct normals", normals, numVertices * 12);
		}
	}

	// end parse

	out = fopen(outfile, "wb");
	if (NULL == out)
	{
		printf("can't open file:%s!", outfile);
		goto END;
	}

	if (NULL != vertices)
	{
		for (int i = 0; i < numVertices; ++i)
		{
			fprintf(out, "v %f %f %f\r\n", vertices[i * 3 + 0], vertices[i * 3 + 1], vertices[i * 3 + 2]);
		}
	}

	if (NULL != uvs)
	{
		for (int i = 0; i < numTexSet; ++i)
		{
			for (int j = 0; j < numVertices; ++j)
			{
				if (i == 0) // only support 1 uv
				{
					fprintf(out, "vt %f %f\r\n", uvs[i][j * 2 + 0], uvs[i][j * 2 + 1]);
				}
			}
		}
	}

	if (NULL != normals)
	{
		for (int i = 0; i < numVertices; ++i)
		{
			fprintf(out, "vn %f %f %f\r\n", normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
		}
	}

	if (NULL != triangles)
	{
		for (int i = 0; i < numTriangles; ++i)
		{
			fprintf(out, "f %d/%d/%d %d/%d/%d %d/%d/%d\r\n", 
				triangles[i * 4 + 0] + 1, triangles[i * 4 + 0] + 1, triangles[i * 4 + 0] + 1,
				triangles[i * 4 + 3] + 1, triangles[i * 4 + 3] + 1, triangles[i * 4 + 3] + 1,
				triangles[i * 4 + 1] + 1, triangles[i * 4 + 1] + 1, triangles[i * 4 + 1] + 1);
		}
	}

	ok = true;

END:

#undef READ_SECTION
#undef READ_VALUE

	if (NULL != in)
	{
		fclose(in);
	}
	if (NULL != out)
	{
		fclose(out);
	}
	if (NULL != triangles)
	{
		free(triangles);
	}
	if (NULL != uvs)
	{
		for (int i = 0; i < numTexSet; ++i)
		{
			if (NULL != uvs[i])
			{
				free(uvs[i]);
			}
		}
		free(uvs);
	}
	if (NULL != vertices)
	{
		free(vertices);
	}
	if (NULL != normals)
	{
		free(normals);
	}

	return ok;
}

bool Bml2Xml(const char* infile, const char* outfile)
{
	bool ok = false;
	FILE* out = NULL;
	char* bytes = NULL;

	FILE* in = fopen(infile, "rb");
	if (NULL == in)
	{
		printf("can't open file:%s!", infile);
		goto END;
	}

	out = fopen(outfile, "wb");
	if (NULL == out)
	{
		printf("can't open file:%s!", outfile);
		goto END;
	}

	fseek(in, 0, SEEK_END);
	int size = ftell(in);
	fseek(in, 0, SEEK_SET);
	
	bytes = (char*)malloc(size);
	if (fread(bytes, size, 1, in) != 1)
	{
		printf("read text failed!");
		goto END;
	}

	for (int i = 0; i < size; ++i)
	{
		bytes[i] = 255 - bytes[i];
	}

	if (fwrite(bytes, size, 1, out) != 1)
	{
		printf("write text failed!");
		goto END;
	}

	ok = true;

END:

#undef READ_SECTION
#undef READ_VALUE

	if (NULL != in)
	{
		fclose(in);
	}
	if (NULL != out)
	{
		fclose(out);
	}
	if (NULL != bytes)
	{
		free(bytes);
	}
	
	return ok;
}

int main(int argc, char** argv) 
{
	if (argc < 3)
	{
		printf(" command: <option> <src> [dst]");
		return 1;
	}

	if (strcmp(argv[1], "-dff2obj") == 0)
	{
		if (argc == 3)
		{
			char path[256];
			strcpy(path, argv[2]);
			strcat(path, ".obj");
			Dff2Obj(argv[2], path);
		}
		else
		{
			Dff2Obj(argv[2], argv[3]);
		}
	}
	else if (strcmp(argv[1], "-bml2xml") == 0)
	{
		if (argc == 3)
		{
			char path[256];
			strcpy(path, argv[2]);
			strcat(path, ".xml");
			Bml2Xml(argv[2], path);
		}
		else
		{
			Bml2Xml(argv[2], argv[3]);
		}
	}

	return 0;
}