#pragma once
#include <vector>
#include <cstddef>
#include "stdint.h"

class ObjLoader {
public:
#ifdef _WIN32
    void load(const wchar_t* filename);
#else
    void load(const char* filename);
#endif
    size_t getIndexCount();
    size_t getVertCount();

    const uint32_t* getFaces();
    const float* getPositions();
    const float* getNormals();
    const float* getTexCoords();
    const float* getTangents();

    bool hasNormals();
    bool hasTangents();

private:
    struct Vec3 {
        float x;
        float y;
        float z;
        Vec3 operator-(const Vec3& rhs);
        Vec3& operator+=(const Vec3& rhs);
        Vec3& operator-=(const Vec3& rhs);
        Vec3& normalize();
        static float dot(const Vec3& lhs, const Vec3& rhs);
        static Vec3 cross(Vec3 lhs, Vec3 rhs);
        Vec3 operator*(float rhs);
    };

    struct Tangent {
        Vec3 t;
        float w;
    };

    struct TexCoord {
        float u;
        float v;
        TexCoord operator-(const TexCoord& rhs);
    };

    struct Face {
        uint32_t a;
        uint32_t b;
        uint32_t c;
    };

    struct ObjVert {
        int32_t vert;
        int32_t norm;
        int32_t coord;
    };
    friend bool operator<(const ObjVert &lhs, const ObjVert &rhs);

    std::vector<Face> Faces;
    std::vector<Vec3> Positions;
    std::vector<Vec3> Normals;
    std::vector<Tangent> Tangents;
    std::vector<TexCoord> TexCoords;  
};
