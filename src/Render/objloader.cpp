#include "objloader.h"

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <map>
#include <cmath>

#ifdef _WIN32
void ObjLoader::load(const wchar_t* filename) {
#else
void ObjLoader::load(const char* filename) {
#endif
    std::ifstream file;
    file.open(filename, std::ios_base::in);
    if (!file.is_open()) {
        return;
    }

    Positions.clear();
    Normals.clear();
    TexCoords.clear();
    Tangents.clear();
    Faces.clear();

    const char *delims = " \n\r";
    char line[1024] = {0};

    std::vector<Vec3> verts;
    std::vector<Vec3> norms;
    std::vector<TexCoord> texcoords;

    std::map<ObjVert, uint32_t> vertMap;
    size_t vertCount = 0;

    while (file.good()) {
        file.getline(line, sizeof(line));
        if (file.eof()) break;

        const char *token = strtok(line, delims);
        if (!token || token[0] == '#' || token[0] == '$')
            continue;

        if (strcmp(token, "v") == 0) {
            float x, y, z, w=1;
            sscanf(line+2, "%f %f %f %f", &x, &y, &z, &w);
            verts.push_back( Vec3{x/w,y/w,z/w} );
        } else if (strcmp(token, "vn") == 0) {
            float x, y, z;
            sscanf(line+3, "%f %f %f", &x, &y, &z);
            norms.push_back( Vec3{x,y,z} );
        } else if (strcmp(token, "vt") == 0) {
            float x, y=0, z=0;
            sscanf(line+3, "%f %f %f", &x, &y, &z);
            texcoords.push_back( TexCoord{x, y} );
        } else if (strcmp(token, "f") == 0) {
            std::vector<int32_t> vertId;
            std::vector<int32_t> normId;
            std::vector<int32_t> texId;

            char *lineptr = line + 2;
            while (lineptr[0] != 0) {
                while (lineptr[0] == ' ') ++lineptr;
                int vi=0, ni=0, ti=0;
                if (sscanf(lineptr, "%d/%d/%d", &vi, &ti, &ni) == 3) {
                    vertId.push_back(vi-1);
                    normId.push_back(ni-1);
                    texId.push_back(ti-1);
                } else if (sscanf(lineptr, "%d//%d", &vi, &ni) == 2) {
                    vertId.push_back(vi-1);
                    normId.push_back(ni-1);
                } else if (sscanf(lineptr, "%d/%d", &vi, &ti) == 2) {
                    vertId.push_back(vi-1);
                    texId.push_back(ti-1);
                } else if (sscanf(lineptr, "%d", &vi) == 1) {
                    vertId.push_back(vi-1);
                }
                while(lineptr[0] != ' ' && lineptr[0] != '\0') ++lineptr;
            }

            for (uint32_t i = 1; i < vertId.size()-1; ++i) {
                ObjVert tri[3] {{vertId[0], normId.empty() ? -1 : normId[0], texId.empty() ? -1 : texId[0]},
                                {vertId[i], normId.empty() ? -1 : normId[i], texId.empty() ? -1 : texId[i]},
                                {vertId[i+1], normId.empty() ? -1 : normId[i+1], texId.empty() ? -1 : texId[i+1]}};
                if (vertMap.count(tri[0]) == 0)
                    vertMap[tri[0]] = vertCount++;
                if (vertMap.count(tri[1]) == 0)
                    vertMap[tri[1]] = vertCount++;
                if (vertMap.count(tri[2]) == 0)
                    vertMap[tri[2]] = vertCount++;

                Faces.push_back(Face{vertMap[tri[0]], vertMap[tri[1]], vertMap[tri[2]]});
            }
        }
    }
    file.close();

    Positions.resize(vertCount);
    if (norms.size() > 0)
        Normals.resize(vertCount);

    std::vector<Vec3> Bitangents;
    if (texcoords.size() > 0) {
        TexCoords.resize(vertCount);
        Tangents.resize(vertCount, {Vec3(), 1.0f});
        Bitangents.resize(vertCount, Vec3());
    }

    for (auto &[face, id] : vertMap) {
        Positions[id] = verts[face.vert];
        if (norms.size() > 0) {
            Normals[id] = norms[face.norm];
        }
        if (texcoords.size() > 0) {
            TexCoords[id] = texcoords[face.coord];
        }
    }

    if (texcoords.size() > 0) {
        for (auto &face : Faces) {
            int i0 = face.a;
            int i1 = face.b;
            int i2 = face.c;
            Vec3 v0 = Positions[i0];
            Vec3 v1 = Positions[i1];
            Vec3 v2 = Positions[i2];
            TexCoord uv0 = TexCoords[i0];
            TexCoord uv1 = TexCoords[i1];
            TexCoord uv2 = TexCoords[i2];

            Vec3 dv1 = v1-v0;
            Vec3 dv2 = v2-v0;
            TexCoord duv1 = uv1-uv0;
            TexCoord duv2 = uv2-uv0;

            float r = 1.0f / (duv1.u * duv2.v - duv1.v * duv2.u);
            Vec3 tangent = (dv1 * duv2.v - dv2 * duv1.v) * r;
            Vec3 bitangent = (dv2 * duv1.u - dv1 * duv2.u) * r;

            Tangents[i0].t += tangent;
            Tangents[i1].t += tangent;
            Tangents[i2].t += tangent;
            Bitangents[i0] += bitangent;
            Bitangents[i1] += bitangent;
            Bitangents[i2] += bitangent;
        }

        for (size_t i = 0; i < Tangents.size(); ++i) {
            Tangents[i].t -= Normals[i] * Vec3::dot(Tangents[i].t, Normals[i]);
            Tangents[i].t.normalize();
            if (Vec3::dot(Vec3::cross(Tangents[i].t, Bitangents[i]), Normals[i]) > 0.0f) {
                Tangents[i].w = -1.0f;
            }
        }
    }
}

ObjLoader::Vec3 ObjLoader::Vec3::operator-(const Vec3& rhs) {
    return Vec3{this->x-rhs.x, this->y-rhs.y, this->z-rhs.z};
}

ObjLoader::Vec3 ObjLoader::Vec3::operator*(float rhs) {
    return Vec3{this->x*rhs, this->y*rhs, this->z*rhs};
}

ObjLoader::TexCoord ObjLoader::TexCoord::operator-(const TexCoord& rhs) {
    return TexCoord{this->u-rhs.u, this->v-rhs.v};
}

size_t ObjLoader::getIndexCount() {
    return 3 * Faces.size();
}

size_t ObjLoader::getVertCount() {
    return Positions.size();
}

bool ObjLoader::hasNormals() {
    return Normals.size() > 0;
}

bool ObjLoader::hasTangents() {
    return Tangents.size() > 0;
}

const uint32_t* ObjLoader::getFaces() {
    return (const uint32_t*) Faces.data();
}

const float* ObjLoader::getPositions() {
    return (const float*) Positions.data();
}

const float* ObjLoader::getNormals() {
    return (const float*) Normals.data();
}

const float* ObjLoader::getTexCoords() {
    return (const float*) TexCoords.data();
}

const float* ObjLoader::getTangents() {
    return (const float*) Tangents.data();
}

ObjLoader::Vec3& ObjLoader::Vec3::operator-=(const Vec3& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
}

ObjLoader::Vec3& ObjLoader::Vec3::operator+=(const Vec3& rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;

    return *this;
}

ObjLoader::Vec3& ObjLoader::Vec3::normalize() {
    float norm = sqrt(x*x+y*y+z*z);
    x /= norm;
    y /= norm;
    z /= norm;
    return *this;
}

ObjLoader::Vec3 ObjLoader::Vec3::cross(Vec3 lhs, Vec3 rhs) {
    return Vec3{lhs.y * rhs.z - lhs.z * rhs.y,
                -(lhs.x * rhs.z - lhs.z * rhs.x),
                lhs.x * rhs.y - lhs.y * rhs.x};
}

float ObjLoader::Vec3::dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z;
}

bool operator<(const ObjLoader::ObjVert &lhs, const ObjLoader::ObjVert &rhs) {
    if (lhs.vert != rhs.vert) return (lhs.vert < rhs.vert);
    if (lhs.norm != rhs.norm) return (lhs.norm < rhs.norm);
    return (lhs.coord < rhs.coord);
}
