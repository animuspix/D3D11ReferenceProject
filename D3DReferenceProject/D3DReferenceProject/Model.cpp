#include "Model.h"
#include "Memory.h"

#include <fstream>
#include <filesystem>

uint32_t modelCtr = 0;

enum MESH_SCAN_MODE
{
	POSITIONS,
	TEXCOORDS,
	NORMALS,
	FACES
};

void Model::Init(const char* path, Vertex3D* vtOutput, uint32_t outputOffset, uint32_t* numVtsLoaded, uint32_t maxVtsPerModel)
{
	// Allocate file data, load file
	const uint64_t fsize = std::filesystem::file_size(path);
	char* data = Memory::AllocateArray<char>(static_cast<uint32_t>(fsize));

	std::fstream strm(path);
	strm.read(data, fsize);

	// Allocate raw attribute data
	float* positions = Memory::AllocateArray<float>(maxVtsPerModel * 3);
	float* texcoords = Memory::AllocateArray<float>(maxVtsPerModel * 3);
	float* normals = Memory::AllocateArray<float>(maxVtsPerModel * 3);

	uint32_t posOffs = 0;
	uint32_t texOffs = 0;
	uint32_t normalOffs = 0;

	uint32_t uvStride = 2; // Two coordinates for Blender, three for 3ds Max
	uint32_t strideCtr = 0; // We want to dynamically calculate the stride to use during file processing

	// Process file into attribute buffers
	//////////////////////////////////////

	MESH_SCAN_MODE currScanMode = POSITIONS;
	bool scanningAttributes = true;
	uint32_t vtsProcessed = 0;

	constexpr uint8_t whitelistLength = 17;
	char whitelist[whitelistLength] = { 'v', 'n', 't', '\n', '-', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'f', '/'};

	// YES this whitelist will skip spaces! That's intentional, they aren't useful to us unless they're a separation character
	// (e.g. delimiting the start/end of a vertex coordinate), so we want to know if `nextChar` is a space (=> we're at the start/end of a number), but if `currChar`
	// is a spaece we can just ignore it, and our parsing algorithm will keep working

	uint32_t step = 1;
	for (uint32_t i = 0; i < fsize - 1; i += step)
	{
		const char currChar = data[i];
		const char nextChar = data[i + 1];

		// Check the current character against our whitelist
		// This can probably be optimized somehow...
		bool whitelisted = false;
		for (uint32_t k = 0; k < whitelistLength && !whitelisted; k++)
		{
			whitelisted |= (currChar == whitelist[k]);
		}

		// Ignore any characters not in our whitelist
		if (whitelisted)
		{
			if (scanningAttributes)
			{
				if (currChar == 'v' && nextChar == ' ')
				{
					// POSITIONS should always be the first attribute in our files, is set by default
					scanningAttributes = false;
				}
				else if (currChar == 'v' && nextChar == 't')
				{
					currScanMode = TEXCOORDS;
					scanningAttributes = false;
				}
				else if (currChar == 'v' && nextChar == 'n')
				{
					currScanMode = NORMALS;
					scanningAttributes = false;
				}
				else if (currChar == 'f' && nextChar == ' ')
				{
					currScanMode = FACES;
					scanningAttributes = false;
				}

				step = 1;
			}
			else
			{
				if (currChar == '\n')
				{
					scanningAttributes = true;
					step = 1;
				}
				else if (nextChar != ' ')
				{
					// Calculate coordinate length in characters
					uint32_t ndxOffs = 0;
					char charProbe = data[i];
					char attribText[64] = {}; // Oversize footprint, just in case any numbers are crazy big (some are! McGuire's bunny.obj has corrupted texture coordinates ;_;)
					while (charProbe != ' ' && charProbe != '\n')
					{
						charProbe = data[i + ndxOffs];
						attribText[ndxOffs] = charProbe;
						ndxOffs++;
					}
					attribText[ndxOffs - 1] = '\0';

					if (currScanMode != FACES)
					{
						// Safety code - not sure whether to keep this
						float coordinate = static_cast<float>(std::atof(attribText));
						coordinate = std::clamp(coordinate, -2.0f, 2.0f);

						// Selectively write out coordinates to attribute buffers
						if (currScanMode == POSITIONS)
						{
							positions[posOffs] = coordinate;
							posOffs++;
						}
						else if (currScanMode == TEXCOORDS)
						{
							texcoords[texOffs] = coordinate;
							texOffs++;
							strideCtr++;

							if (data[i + ndxOffs - 1] == '\n')
							{
								uvStride = strideCtr;
								strideCtr = 0;
							}
						}
						else if (currScanMode == NORMALS)
						{
							normals[normalOffs] = coordinate;
							normalOffs++;
						}
					}
					else
					{
						// Position, texture, normal indices needed for de-indexation when we're scanning faces, zero otherwise
						uint32_t slashCtr = 0; // Indices within each OBJ face are delimited by slashes
						uint32_t attribNdces[3] = {};

						uint32_t deIndexer = 0;
						char attribNdxText[16] = {};
						uint8_t attribNdxWriter = 0;

						bool ndcesScanning = true;
						while (ndcesScanning)
						{
							ndcesScanning = attribText[deIndexer] != '\0';

							if (attribText[deIndexer] == '/' || !ndcesScanning) // Searching for slashes only skips the last index in each face
							{
								attribNdces[slashCtr] = atoi(attribNdxText) - 1; // OBJ indices are 1-based
								attribNdxWriter = 0;
								slashCtr++;
							}
							else
							{
								attribNdxText[attribNdxWriter] = attribText[deIndexer];
								attribNdxWriter++;
							}
							deIndexer++;
						}

						// Re-duplicate vertices, and encode the results in our vertex output buffer
						const uint32_t posNdx = attribNdces[0] * 3;
						const uint32_t uvNdx = attribNdces[1] * uvStride;
						const uint32_t normNdx = attribNdces[2] * 3;

						memcpy(&vtOutput[vtsProcessed].pos, &positions[posNdx], sizeof(float) * 3);
						vtOutput[vtsProcessed].pos.w = 0;

						memcpy(&vtOutput[vtsProcessed].mat, &texcoords[uvNdx], sizeof(float) * 2);
						vtOutput[vtsProcessed].mat.z = MATERIAL_TYPES::DIFFUSE;
						vtOutput[vtsProcessed].mat.w = static_cast<float>(modelCtr);

						memcpy(&vtOutput[vtsProcessed].normals, &normals[normNdx], sizeof(float) * 3);
						vtOutput[vtsProcessed].normals.w = 0;

						vtsProcessed++;
					}

					step = ndxOffs - 1;
				}
				else
				{
					step = 1;
				}
			}
		}
		else
		{
			step = 1;
		}
	}

	*numVtsLoaded = vtsProcessed;
	modelCtr++;

	Memory::FreeToAddress(data);
}
