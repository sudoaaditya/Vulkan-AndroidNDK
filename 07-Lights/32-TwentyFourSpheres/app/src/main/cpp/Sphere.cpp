#include "Sphere.h"

#include <cmath>
#include <cstring>

namespace {

constexpr unsigned int kRings = 19;    // rings between poles
constexpr unsigned int kSectors = 20;  // segments around Y axis
constexpr unsigned int kVertexCount = 2 + (kRings * kSectors);
constexpr unsigned int kIndexCount = 2280;
constexpr float kPi = 3.14159265358979323846f;

static float gSpherePositions[1146];
static float gSphereNormals[1146];
static float gSphereTexcoords[764];
static unsigned short gSphereElements[2280];
static bool gGenerated = false;

inline void set_vertex(unsigned int index, float x, float y, float z, float u, float v) {
    gSpherePositions[(index * 3) + 0] = x;
    gSpherePositions[(index * 3) + 1] = y;
    gSpherePositions[(index * 3) + 2] = z;

    gSphereNormals[(index * 3) + 0] = x;
    gSphereNormals[(index * 3) + 1] = y;
    gSphereNormals[(index * 3) + 2] = z;

    gSphereTexcoords[(index * 2) + 0] = u;
    gSphereTexcoords[(index * 2) + 1] = v;
}

void generate_sphere() {
    if (gGenerated) {
        return;
    }

    // Top and bottom pole.
    set_vertex(0, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f);
    set_vertex(kVertexCount - 1, 0.0f, -1.0f, 0.0f, 0.5f, 0.0f);

    // Interior ring vertices (no seam-duplicate vertices).
    unsigned int vertex = 1;
    for (unsigned int ring = 0; ring < kRings; ++ring) {
        const float v = static_cast<float>(ring + 1) / static_cast<float>(kRings + 1);
        const float theta = v * kPi;
        const float y = std::cos(theta);
        const float r = std::sin(theta);

        for (unsigned int sector = 0; sector < kSectors; ++sector) {
            const float u = static_cast<float>(sector) / static_cast<float>(kSectors);
            const float phi = u * 2.0f * kPi;

            const float x = r * std::cos(phi);
            const float z = r * std::sin(phi);

            set_vertex(vertex, x, y, z, u, 1.0f - v);
            ++vertex;
        }
    }

    unsigned int e = 0;

    // Top cap.
    for (unsigned int sector = 0; sector < kSectors; ++sector) {
        const unsigned short a = 0;
        const unsigned short b = static_cast<unsigned short>(1 + sector);
        const unsigned short c = static_cast<unsigned short>(1 + ((sector + 1) % kSectors));
        gSphereElements[e++] = a;
        gSphereElements[e++] = b;
        gSphereElements[e++] = c;
    }

    // Middle quads (as two triangles per quad).
    for (unsigned int ring = 0; ring < (kRings - 1); ++ring) {
        const unsigned int row0 = 1 + (ring * kSectors);
        const unsigned int row1 = row0 + kSectors;

        for (unsigned int sector = 0; sector < kSectors; ++sector) {
            const unsigned short a = static_cast<unsigned short>(row0 + sector);
            const unsigned short b = static_cast<unsigned short>(row0 + ((sector + 1) % kSectors));
            const unsigned short c = static_cast<unsigned short>(row1 + sector);
            const unsigned short d = static_cast<unsigned short>(row1 + ((sector + 1) % kSectors));

            gSphereElements[e++] = a;
            gSphereElements[e++] = c;
            gSphereElements[e++] = b;

            gSphereElements[e++] = b;
            gSphereElements[e++] = c;
            gSphereElements[e++] = d;
        }
    }

    // Bottom cap.
    const unsigned short southPole = static_cast<unsigned short>(kVertexCount - 1);
    const unsigned int lastRow = 1 + ((kRings - 1) * kSectors);
    for (unsigned int sector = 0; sector < kSectors; ++sector) {
        const unsigned short a = static_cast<unsigned short>(lastRow + sector);
        const unsigned short b = static_cast<unsigned short>(lastRow + ((sector + 1) % kSectors));
        const unsigned short c = southPole;
        gSphereElements[e++] = a;
        gSphereElements[e++] = c;
        gSphereElements[e++] = b;
    }

    // Safety guard: keep data deterministic if algorithm changes in future.
    if (e < kIndexCount) {
        std::memset(gSphereElements + e, 0, sizeof(unsigned short) * (kIndexCount - e));
    }

    gGenerated = true;
}

}  // namespace

extern "C" void getSphereVertexData(float spherePositionCoords[1146],
                                     float sphereNormalCoords[1146],
                                     float sphereTexCoords[764],
                                     unsigned short sphereElements[2280]) {
    generate_sphere();

    std::memcpy(spherePositionCoords, gSpherePositions, sizeof(gSpherePositions));
    std::memcpy(sphereNormalCoords, gSphereNormals, sizeof(gSphereNormals));
    std::memcpy(sphereTexCoords, gSphereTexcoords, sizeof(gSphereTexcoords));
    std::memcpy(sphereElements, gSphereElements, sizeof(gSphereElements));
}

extern "C" unsigned int getNumberOfSphereVertices(void) {
    return kVertexCount;
}

extern "C" unsigned int getNumberOfSphereElements(void) {
    return kIndexCount;
}
