#include <atomic>
#ifndef PUBGM_PHYSX_H
#define PUBGM_PHYSX_H

#include <unordered_map>
#include <chrono>
#include <functional>
#include <thread>
#include <cstdlib>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <initializer_list>
#include <dirent.h>
#include "Log.h"
#include "struct.h"
#include "DrawOnlyMemoryBridge.h"
#include "DrawOnlyPhysXThrottle.h"
#include "DrawOnlySceneBounds.h"
#include "版本数据_1_39.h"
// PhysX.h
#pragma once



using namespace std;
inline std::atomic<int> target_pid{-1};
inline std::atomic<uint64_t> PhysxInstancePtr{0};
typedef unsigned long u64;
typedef unsigned int u32;

inline uintptr_t ReadModule(const char *module_name) {
    return draw_only_memory::ModuleBase(module_name);
}

inline int ReadProcess(const char* packageName) {
    return draw_only_memory::FindPid(packageName);
}

// Scene visibility uses reads only; failed or partial reads return cleared data.
inline bool GetAddress(unsigned long address, void* buffer, size_t size) {
    if (!buffer || size == 0) return false;
    if (target_pid <= 0) { memset(buffer, 0, size); return false; }
    return draw_only_memory::Read(address, buffer, size);
}

template <class T>
inline T read(uintptr_t address) {
    T value{};
    GetAddress(address, &value, sizeof(T));
    return value;
}

template <typename T>
struct TArray
{
    uintptr_t base;
    int32_t count;
    int32_t max;

    std::vector<T> ToVec() const
    {
        if (!IsValid() || count == 0)
        {
            return {};
        }
        std::vector<T> vec{};
        vec.resize(static_cast<size_t>(count));
        iovec l_iov{&vec[0], static_cast<size_t>(count) * sizeof(T)};
        iovec r_iov{reinterpret_cast<void*>(base), static_cast<size_t>(count) * sizeof(T)};
        return vec;
    }

    T operator[](size_t u) const
    {
        return read<T>(base + u * sizeof(T));
    }

    bool IsValid() const
    {
        return draw_only_scene_bounds::ArrayValid(base, count, max,
            draw_only_scene_bounds::MaxActors, sizeof(T));
    }
};


#include <set>        // std::set
#include <numeric>    // std::accumulate
#include <sstream>    // std::ostringstream
#include "PxTransform.h"
#include "PxVec3.h"
#include "PxVec4.h"

// 1.39 用户提供的 PhysX 实例偏移，统一放在版本数据文件维护。
inline constexpr uintptr_t OffsetPhysxInstancePtr = V139::PhysX实例;
#define SQManager 0x2430
#define RigidActors 0x2568

enum class PxGeometryType : int32_t
{
    eSPHERE        = 0, // 球体
    ePLANE         = 1, // 平面
    eCAPSULE       = 2, // 胶囊体（圆柱 + 半球两端）
    eBOX           = 3, // 盒子（立方体或矩形体）
    eCONVEXMESH    = 4, // 凸包网格（凸面多边形网格）
    eTRIANGLEMESH  = 5, // 三角形网格（可凹多边形，常用于静态物体）
    eHEIGHTFIELD   = 6, // 高度图（用于地形碰撞）
    eGEOMETRY_COUNT, //!< internal use only!
    eINVALID = -1	 //!< internal use only!
};

struct FIntVector2D {
    int X, Y;
};

struct FilterDataT
{
    uint32_t word0;
    uint32_t word1;
    uint32_t word2; 
    uint32_t word3;
};

struct PrunerPayload
{
    uint64_t Shape;
    uint64_t Actor;
    bool operator==(const PrunerPayload& other) const {
        return Shape == other.Shape && Actor == other.Actor;
    }
    bool operator<(const PrunerPayload& other) const {
        return std::tie(Shape, Actor) < std::tie(other.Shape, other.Actor);
    }
};

struct PrunerPayloadHash {
    size_t operator()(const PrunerPayload& p) const {
        return std::hash<uint64_t>()(p.Shape) ^ (std::hash<uint64_t>()(p.Actor) << 1);
    }
};

struct Int64Hash {
    size_t operator()(const uint64_t& p) const {
        return std::hash<uint64_t>()(p);
    }
};

struct TriangleMeshData
{
    std::vector<physx::PxVec3> Vertices{};
    std::vector<uint32_t> Indices{};
    uint8_t Flags{};
    FilterDataT QueryFilterData{};
    FilterDataT SimulationFilterData{};
    PrunerPayload UniqueKey1;
    uint64_t UniqueKey2;
    PrunerPayload UniqueKey3;
    PxGeometryType Type{};
    physx::PxTransform Transform;
};

namespace PhysX
{
    enum class PxConcreteType : uint16_t
    {

        eUNDEFINED,

        eHEIGHTFIELD,
        eCONVEX_MESH,
        eTRIANGLE_MESH_BVH33,
        eTRIANGLE_MESH_BVH34,
        eCLOTH_FABRIC,

        eRIGID_DYNAMIC,
        eRIGID_STATIC,
        eSHAPE,
        eMATERIAL,
        eCONSTRAINT,
        eCLOTH,
        ePARTICLE_SYSTEM,
        ePARTICLE_FLUID,
        eAGGREGATE,
        eARTICULATION,
        eARTICULATION_LINK,
        eARTICULATION_JOINT,
        ePRUNING_STRUCTURE,

        ePHYSX_CORE_COUNT,
        eFIRST_PHYSX_EXTENSION = 256,
        eFIRST_VEHICLE_EXTENSION = 512,
        eFIRST_USER_EXTENSION = 1024
    };

    enum class PxBaseFlag : uint16_t
    {
        eOWNS_MEMORY = (1 << 0),
        eIS_RELEASABLE = (1 << 1)
    };

    enum class PxRigidBodyFlag : uint8_t
    {

        eKINEMATIC = (1 << 0), //!< Enable kinematic mode for the body.

        eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES = (1 << 1),

        eENABLE_CCD = (1 << 2), //!< Enable CCD for the body.

        eENABLE_CCD_FRICTION = (1 << 3),

        eENABLE_POSE_INTEGRATION_PREVIEW = (1 << 4),

        eENABLE_SPECULATIVE_CCD = (1 << 5),

        eENABLE_CCD_MAX_CONTACT_IMPULSE = (1 << 6)
    };

    struct PxMatrix3x3T
    {

        physx::PxVec3 column0{};
        physx::PxVec3 column1{};
        physx::PxVec3 column2{};

        PxMatrix3x3T() = default;

        PxMatrix3x3T(const physx::PxVec4& q)
        {
            const float x = q.x;
            const float y = q.y;
            const float z = q.z;
            const float w = q.w;

            const float x2 = x + x;
            const float y2 = y + y;
            const float z2 = z + z;

            const float xx = x2 * x;
            const float yy = y2 * y;
            const float zz = z2 * z;

            const float xy = x2 * y;
            const float xz = x2 * z;
            const float xw = x2 * w;

            const float yz = y2 * z;
            const float yw = y2 * w;
            const float zw = z2 * w;

            column0 = physx::PxVec3(1.0f - yy - zz, xy + zw, xz - yw);
            column1 = physx::PxVec3(xy - zw, 1.0f - xx - zz, yz + xw);
            column2 = physx::PxVec3(xz + yw, yz - xw, 1.0f - xx - yy);
        }

        PxMatrix3x3T(const physx::PxVec3& col0, const physx::PxVec3& col1, const physx::PxVec3& col2)
                : column0(col0), column1(col1), column2(col2)
        {
        }

        const PxMatrix3x3T getTranspose() const
        {
            const physx::PxVec3 v0(column0.x, column1.x, column2.x);
            const physx::PxVec3 v1(column0.y, column1.y, column2.y);
            const physx::PxVec3 v2(column0.z, column1.z, column2.z);

            return PxMatrix3x3T(v0, v1, v2);
        }

        const physx::PxVec3 transform(const physx::PxVec3& other) const
        {
            return column0 * other.x + column1 * other.y + column2 * other.z;
        }

        const PxMatrix3x3T operator*(const PxMatrix3x3T& other) const
        {
            // Rows from this <dot> columns from other
            // column0 = transform(other.column0) etc
            return PxMatrix3x3T(transform(other.column0), transform(other.column1), transform(other.column2));
        }

        inline const physx::PxVec3 operator*(const physx::PxVec3& vec) const
        {
            return transform(vec);
        }
    };

    //struct PxsRigidCore
    struct PxsRigidCoreT
    {
        alignas(16)
        physx::PxTransform mBodyToWorld alignas(16);
        PxRigidBodyFlag Flags{};// API body flags
        uint8_t m_idt_body_to_actor{};// PT: true if PxsBodyCore::body2Actor is identity
        uint16_t m_solver_iteration_counts{};//vel iters are in low word and pos iters in high word.
    };

    struct BodyCoreT
    {
        char mPad[0x10]{};
        alignas(16) PxsRigidCoreT mCore {};
        alignas(16) physx::PxTransform mBodyToActor;
    };

    // rigid base class
    struct BodyT
    {
        char mPad[0x60]{};
        uint64_t mScene{};
        uint64_t mControlState{};
        uint64_t mStreamPtr{};
        BodyCoreT mRigid{};
    };


    struct PxActorT
    {
        //= ATTENTION! =====================================================================================
        // Changing the data layout of this class breaks the binary serialization format.  See comments for
        // PX_BINARY_SERIAL_VERSION.  If a modification is required, please adjust the getBinaryMetaData
        // function.  If the modification is made on a custom branch, please change PX_BINARY_SERIAL_VERSION
        // accordingly.
        //==================================================================================================
        char mPad[0x8]{};
        PxConcreteType mType{};// concrete type identifier - see PxConcreteType.
        PxBaseFlag mBaseFlags{};// internal flags
    };

    struct PxGeometryT
    {
        PxGeometryType mType{};
    };

    struct PxBoxGeometry : PxGeometryT
    {

        // 盒子宽度、高度和深度的一半。
        physx::PxVec3 mHalfExtents{};

        bool valid()
        {

            if (mType != PxGeometryType::eBOX) return false;
            // if (!halfExtents.isFinite())
            //	return false;
            if (mHalfExtents.x <= 0.0f || mHalfExtents.y <= 0.0f || mHalfExtents.z <= 0.0f) return false;

            return true;
        }
    };

    struct PxSphereGeometryT : PxGeometryT
    {
        float mRadius{};

        bool valid()
        {

            if (mType != PxGeometryType::eSPHERE) return false;
            // if (!halfExtents.isFinite())
            //	return false;
            if (mRadius <= 0.f) return false;
            return true;
        }
    };

    struct PxCapsuleGeometryT : PxGeometryT
    {

        float mRadius{};
        float mHalfHeight{};

        bool valid()
        {

            if (mType != PxGeometryType::eCAPSULE)
                return false;
            // if (!halfExtents.isFinite())
            //	return false;
            if (mRadius <= 0.f || mHalfHeight <= 0.f)
                return false;

            return true;
        }
    };

    struct PxMeshScale
    {
    public:
        physx::PxVec3 transform(const physx::PxVec3& v) const
        {
            return rotation.rotateInv(scale.multiply(rotation.rotate(v)));
        }

        physx::PxVec3 scale;	  //!< A nonuniform scaling
        physx::PxQuat rotation; //!< The orientation of the scaling axes
    };

    struct CenterExtentsT
    {

        physx::PxVec3 mCenter{};
        physx::PxVec3 mExtents{};
    };

    struct PxPlaneT
    {
        physx::PxVec3 n; //!< The normal to the plane
        float d;  //!< The distance from the origin
        bool operator==(const PxPlaneT& p) const
        {
            return n == p.n && d == p.d;
        }
    };

    struct HullPolygonDataT
    {

        PxPlaneT mPlane; //!< Plane equation for this polygon	//Could drop 4th elem as it can be computed from any vertex as: d = - p.dot(n);
        uint16_t mVRef8;   //!< Offset of vertex references in hull vertex data (CS: can we assume indices are tightly packed and offsets are ascending?? DrawObjects makes and uses this assumption)
        uint8_t mNbVerts;  //!< Number of vertices/edges in the polygon
        uint8_t mMinIndex; //!< Index of the polygon vertex that has minimal projection along this plane's normal.
    };

    template <typename storageType, storageType bitMask>
    class PxBitAndDataT
    {
    public:
        PxBitAndDataT(const physx::PxEMPTY)
        {
        }
        PxBitAndDataT() : mData(0)
        {
        }
        PxBitAndDataT(storageType data, bool bit = false)
        {
            mData = bit ? storageType(data | bitMask) : data;
        }

        operator storageType() const
        {
            return storageType(mData & ~bitMask);
        }
        void setBit()
        {
            mData |= bitMask;
        }
        void clearBit()
        {
            mData &= ~bitMask;
        }
        storageType isBitSet() const
        {
            return storageType(mData & bitMask);
        }

    protected:
        storageType mData;
    };
    typedef PxBitAndDataT<unsigned char, 0x80> PxBitAndByte;
    typedef PxBitAndDataT<unsigned short, 0x8000> PxBitAndWord;
    typedef PxBitAndDataT<unsigned int, 0x80000000> PxBitAndDword;
    template<uint8_t TNumBytes>
    struct PxPadding
    {
        uint8_t mPadding[TNumBytes];
        PxPadding()
        {
            for ( uint8_t idx =0; idx < TNumBytes; ++idx )
                mPadding[idx] = 0;
        }
    };
    struct ConvexHullDataT
    {
        CenterExtentsT mAABB{};//+0x20
        physx::PxVec3 m_center_of_mass{};//+0x38
        PxBitAndWord mNbEdges{};//
        uint8_t HullVerticesNb{};//+0x46
        uint8_t PolygonsNb{};//+0x47
        HullPolygonDataT* mPolygons{};//0x48

        const physx::PxVec3* getHullVertices() const //!< Convex hull vertices
        {
            uintptr_t address = 0;
            if (!draw_only_scene_bounds::AddressOffset(reinterpret_cast<uintptr_t>(mPolygons),
                    sizeof(HullPolygonDataT) * PolygonsNb, address)) return nullptr;
            return reinterpret_cast<const physx::PxVec3*>(address);
        }

        const uint8_t* getVertexData8() const //!< Vertex indices indexed by hull polygons
        {
            size_t offset = sizeof(HullPolygonDataT) * PolygonsNb;
            offset += sizeof(physx::PxVec3) * HullVerticesNb;
            offset += sizeof(uint8_t) * static_cast<uint16_t>(mNbEdges) * 2;
            offset += sizeof(uint8_t) * HullVerticesNb * 3;
            if (mNbEdges.isBitSet()) offset += sizeof(uint16_t) * static_cast<uint16_t>(mNbEdges) * 2;
            uintptr_t address = 0;
            if (!draw_only_scene_bounds::AddressOffset(reinterpret_cast<uintptr_t>(mPolygons), offset, address)) return nullptr;
            return reinterpret_cast<const uint8_t*>(address);
        }

        const uint8_t* getIndexBuffer() const
        {

            int64_t v1;			// r10
            const char* result; // rax

            v1 = mNbEdges & 0x7FFF;
            result = reinterpret_cast<const char*>(mPolygons) + 15 * HullVerticesNb + 2 * v1;
            if ((mNbEdges & 0x8000u) != 0)
                result += 4 * v1;
            return reinterpret_cast<const uint8_t*>(result);
        }

        const uint8_t* getFacesByEdges8() const //!< for each edge, gives 2 adjacent polygons; used by convex-convex code to come up with all the convex' edge normals.
        {

            const char* tmp = reinterpret_cast<const char*>(mPolygons);
            tmp += 20 * PolygonsNb;
            tmp += 12 * HullVerticesNb;
            return reinterpret_cast<const uint8_t*>(tmp);
        }

        const uint8_t* getFacesByVertices8() const //!< for each edge, gives 2 adjacent polygons; used by convex-convex code to come up with all the convex' edge normals.
        {

            const char* tmp = reinterpret_cast<const char*>(mPolygons);
            tmp += 20 * PolygonsNb;
            tmp += 12 * HullVerticesNb;
            tmp += 1 * mNbEdges * 2;
            return reinterpret_cast<const uint8_t*>(tmp);
        }
    };

    struct PxConvexMeshT
    {
    };
    struct ConvexMeshT : PxConvexMeshT
    {
        //= ATTENTION! =====================================================================================
        // Changing the data layout of this class breaks the binary serialization format.  See comments for
        // PX_BINARY_SERIAL_VERSION.  If a modification is required, please adjust the getBinaryMetaData
        // function.  If the modification is made on a custom branch, please change PX_BINARY_SERIAL_VERSION
        // accordingly.
        //==================================================================================================
        char mPad[0x8]{};
        PxConcreteType mType{};
        PxBaseFlag mBaseFlags{};
        uint64_t mRefCountableVfptr{};
        int32_t mRefCount{};
        ConvexHullDataT HullData{};
        uint32_t mNb{};// ### PT: added for serialization. Try to remove later?
    };

    struct PxConvexMeshGeometryT : PxGeometryT
    {
        PxMeshScale Scale{};//!< The scaling transformation (from vertex space to shape space).
        ConvexMeshT* ConvexMesh{};//!< A reference to the convex mesh object.
        float mMaxMargin{};//!< Max shrunk amount permitted by PCM contact gen
        uint8_t mMeshFlags{};//!< Mesh flags.
        //char mFlagPad[0x3]{};
        PxPadding<3> paddingFromFlags;	//!< padding for mesh flags
    };

    // Possible optimization: align the whole struct to cache line
    struct TriangleMeshT
    {
        char mPad[0x8]{};//
        PxConcreteType mType{};
        PxBaseFlag mBaseFlags{};
        uint64_t mRefCountableVfptr{};
        int32_t mRefCount{};
        uint32_t mNbVertices{};//
        uint32_t mNbTriangles{};//!< 16 (<= 0xffff #vertices) or 32 bit trig indices (mNbTriangles * 3)
        // 16 bytes block
        // PT: WARNING: bounds must be followed by at least 32bits of data for safe SIMD loading
        physx::PxVec3* Vertices{};//
        void* mTriangles{};//
        CenterExtentsT mAABB{};//
        uint8_t* mExtraTrigData{};// PT: WARNING: bounds must be followed by at least 32bits of data for safe SIMD loading
        float mGeomEpsilon{};//!< see comments in cooking code referencing this variable
        /*
        low 3 bits (mask: 7) are the edge flags:
        b001 = 1 = ignore edge 0 = edge v0-->v1
        b010 = 2 = ignore edge 1 = edge v0-->v2
        b100 = 4 = ignore edge 2 = edge v1-->v2
        */
        uint8_t Flags{};//!< Flag whether indices are 16 or 32 bits wide
    };

    struct PxTriangleMeshGeometryT : PxGeometryT
    {
        PxMeshScale Scale{};//!< The scaling transformation.
        uint8_t mMeshFlags{};//!< Mesh flags.
        PxPadding<3> paddingFromFlags;	//!< padding for mesh flags
        TriangleMeshT* mTriangleMesh{};//!< A reference to the mesh object.
    };

    struct PxHeightFieldSampleT
    {

        int16_t mHeight{};
        PxBitAndByte mMaterialIndex0{};
        PxBitAndByte mMaterialIndex1{};
    };

    struct HeightFieldDataT
    {
        CenterExtentsT mAABB{};
        uint32_t Rows{};// PT: WARNING: don't change this member's name (used in ConvX)
        uint32_t Columns{};// PT: WARNING: don't change this member's name (used in ConvX)
        float mRowLimit{};// PT: to avoid runtime int-to-float conversions on Xbox
        float mColumnLimit{};// PT: to avoid runtime int-to-float conversions on Xbox
        float mNbColumns{};// PT: to avoid runtime int-to-float conversions on Xbox
        PxHeightFieldSampleT* mSamples{};// PT: WARNING: don't change this member's name (used in ConvX)
        float mThickness{};
        float mConvexEdgeThreshold{};
        uint16_t mFlags{};
        uint8_t mFormat{};
    };

    struct HeightFieldT
    {
        char mPad[0x8]{};
        PxConcreteType mType{};
        PxBaseFlag mBaseFlags{};
        uint64_t mRefCountableVfptr{};
        int32_t mRefCount{};
        HeightFieldDataT mData{};
        uint32_t mSampleStride{};
        uint32_t mNbSamples{};// PT: added for platform conversion. Try to remove later.
        float mMinHeight{};
        float mMaxHeight{};
        int32_t mModifyCount{};
        // methods
        void* mMeshFactory{};// PT: changed to pointer for serialization
    };

    struct PxHeightFieldGeometryT : PxGeometryT
    {
        HeightFieldT* mHeightField{};
        float HeightScale{};
        float RowScale{};
        float ColumnScale{};
        int8_t Flags{};
        PxPadding<3> paddingFromFlags;	//!< padding for mesh flags.
    };

    struct GeometryUnionT
    {

        union
        {
            void* alignment; // PT: Makes sure the class is at least aligned to pointer size. See DE6803.
            uint8_t box[16];
            uint8_t sphere[8];
            uint8_t capsule[12];
            uint8_t plane[4];
            uint8_t convex[64];
            uint8_t mesh[80];
            uint8_t heightfield[56];
            uint8_t invalid[4];
        } mGeometry;

        PxGeometryType getType() const { return reinterpret_cast<const PxGeometryT&>(mGeometry).mType; }
    };

    struct PxShapeCoreT
    {

        alignas(16) physx::PxTransform transform;
        float contactOffset;
        uint8_t mShapeFlags;			// !< API shape flags	// PT: TODO: use PxShapeFlags here. Needs to move flags to separate file.
        uint8_t mOwnsMaterialIdxMemory; // PT: for de-serialization to avoid deallocating material index list. Moved there from Sc::ShapeCore (since one byte was free).
        uint16_t materialIndex;
        GeometryUnionT geometry;
    };

    struct ShapeCoreT
    {

        FilterDataT QueryFilterData{};
        FilterDataT SimulationFilterData{};
        alignas(16) PxShapeCoreT mCore {};
        float mRestOffset{};

        PxGeometryType getGeometryType() const { return mCore.geometry.getType(); }
        const physx::PxTransform getShape2Actor() const { return mCore.transform; }
    };

    struct ShapeT
    {
        char mPad[0x30]{};
        uint64_t mScene{};
        uint32_t mControlState{};
        uint64_t mStreamPtr{};
        ShapeCoreT ShapeCore{};
        const PxGeometryT& GetGeometry() const { return reinterpret_cast<const PxGeometryT&>(ShapeCore.mCore.geometry.mGeometry); }
    };

    struct PxBounds3
    {
        physx::PxVec3 min, max;
    };


    // Pool of AABBs
    struct PruningPoolT
    {
        uint32_t mNbObjects;
        uint32_t mMaxNbObjects;
        PxBounds3* mWorldBoxes;
        PrunerPayload* mObjects;
    };

    //// PT: extended pruner structure. We might want to move the additional data to the pruner itself later.
    struct PrunerExtT
    {
        uint64_t mPruner;
        char mPad[0x24]{};
        uint32_t mTimestamp;
    };

    struct NpSceneT
    {
        char mPad[SQManager]{};//0x2568
        PrunerExtT exts[2];
    };

    struct ShapeDataT
    {
        ShapeT Shape{};
        BodyT Actor{};
        PxGeometryType mType{};
        uint8_t m_shape_flags{};
        PxMeshScale Scale{};
        PrunerPayload UniqueKey;

        // ConvexData
        ConvexMeshT ConvexMesh{};
        std::vector<HullPolygonDataT> polygons{};
        std::vector<uint8_t> ConvexIndices{};

        // TriangleData
        std::vector<uint32_t> Indices{};
        std::vector<uint16_t> SmallIndices{};
        TriangleMeshT TriangleMesh{};

        // ConvexData & TriangleData
        std::vector<physx::PxVec3> Vertices{};

        // HeightFieldData
        std::vector<PxHeightFieldSampleT> mSamples{};
        HeightFieldT mHeightField{};
    };

}// namespace physx

template<typename T, class Count>
inline bool AddReadVecScatter(uint64_t address, Count size, std::vector<T>* vec,
                              size_t limit = draw_only_scene_bounds::MaxIndices)
{
    return vec && draw_only_scene_bounds::ReadVector(address, size, limit, *vec,
        [](uintptr_t source, void* output, size_t bytes) { return GetAddress(source, output, bytes); });
}

inline physx::PxTransform MeshPose(const PhysX::ShapeDataT& data, bool dynamic) {
    auto global = data.Actor.mRigid.mCore.mBodyToWorld;
    if (dynamic) global = global * data.Actor.mRigid.mBodyToActor.getInverse();
    return global * data.Shape.ShapeCore.mCore.transform;
}
inline bool MeshPoseFinite(const physx::PxTransform& pose) {
    using draw_only_scene_bounds::Finite;
    return Finite(pose.p.x) && Finite(pose.p.y) && Finite(pose.p.z) &&
           Finite(pose.q.x) && Finite(pose.q.y) && Finite(pose.q.z) && Finite(pose.q.w);
}
inline bool MeshPoseSame(const physx::PxTransform& a, const physx::PxTransform& b) {
    constexpr float tolerance = 0.1f;
    return std::abs(a.p.x - b.p.x) <= tolerance && std::abs(a.p.y - b.p.y) <= tolerance &&
           std::abs(a.p.z - b.p.z) <= tolerance && std::abs(a.q.x - b.q.x) <= tolerance &&
           std::abs(a.q.y - b.q.y) <= tolerance && std::abs(a.q.z - b.q.z) <= tolerance &&
           std::abs(a.q.w - b.q.w) <= tolerance;
}
inline uint64_t MeshHeaderRevision(const PhysX::ShapeDataT& data) {
    // Include dimensions/scales/type/flags as well as the backing mesh pointer.
    // Do not reinterpret boxes or convex geometry as triangle pointer layouts.
    size_t bytes = sizeof(PhysX::PxGeometryT);
    switch (data.mType) {
        case PxGeometryType::eBOX: bytes = sizeof(PhysX::PxBoxGeometry); break;
        case PxGeometryType::eTRIANGLEMESH: bytes = sizeof(PhysX::PxTriangleMeshGeometryT); break;
        case PxGeometryType::eCONVEXMESH: bytes = sizeof(PhysX::PxConvexMeshGeometryT); break;
        default: break;
    }
    uint64_t revision = 14695981039346656037ull;
    const auto hash = [&](const void* source, size_t count) {
        const auto* input = static_cast<const uint8_t*>(source);
        for (size_t i = 0; i < count; ++i) { revision ^= input[i]; revision *= 1099511628211ull; }
    };
    hash(&data.Shape.GetGeometry(), bytes);
    hash(&data.m_shape_flags, sizeof(data.m_shape_flags));
    hash(&data.Shape.ShapeCore.QueryFilterData, sizeof(FilterDataT));
    hash(&data.Shape.ShapeCore.SimulationFilterData, sizeof(FilterDataT));
    return revision;
}

template <class _Pr>
inline std::vector<TriangleMeshData> GetMeshData(
        std::vector<PrunerPayload>& objects,
        _Pr filter,
        bool isDynamic = false,
        bool autoTransform = true,
        draw_only_scene_bounds::LoadReport<PrunerPayload>* report = nullptr
) {
    std::vector<PhysX::ShapeDataT> ShapeDatas{};
    std::vector<TriangleMeshData> TriangleMeshDatas{};
    if (report) report->Begin(objects);
    if (objects.size() > draw_only_scene_bounds::MaxActors) return {};
    // Using index iteration instead of reference

    ShapeDatas.reserve(objects.size());
    for (size_t i = 0; i < objects.size(); i++)
    {
        auto obj = objects[i];
        PhysX::ShapeDataT data{};
        data.UniqueKey = obj;
        if (!GetAddress(obj.Shape, &data.Shape, sizeof(PhysX::ShapeT)) ||
            !GetAddress(obj.Actor, &data.Actor, sizeof(PhysX::BodyT))) continue;
        if (!MeshPoseFinite(MeshPose(data, isDynamic))) continue;
        ShapeDatas.push_back(std::move(data));
    }

    for (auto& ShapeData : ShapeDatas)
    {
        ShapeData.m_shape_flags = ShapeData.Shape.ShapeCore.mCore.mShapeFlags;
        ShapeData.mType = ShapeData.Shape.ShapeCore.getGeometryType();
//printf("ShapeData.m_shape_flags %d, ShapeData.mType %d \n",ShapeData.m_shape_flags,ShapeData.mType);
    }

    ShapeDatas.erase(
            std::remove_if(ShapeDatas.begin(), ShapeDatas.end(), [&](const PhysX::ShapeDataT& data) {
                if (data.mType < PxGeometryType::eSPHERE || data.mType >= PxGeometryType::eGEOMETRY_COUNT)
                    return true; // Invalid headers remain pending, never confirmed filtered.
                if (!filter(data)) return false;
                if (report) report->Filtered(data.UniqueKey);
                return true;
            }),ShapeDatas.end()
    );

    for (auto& ShapeData : ShapeDatas)
    {
        if (ShapeData.mType == PxGeometryType::eHEIGHTFIELD)
        {
            PhysX::PxHeightFieldGeometryT field_geometry = (PhysX::PxHeightFieldGeometryT&)ShapeData.Shape.GetGeometry();
            if (!GetAddress((uint64_t)field_geometry.mHeightField, &ShapeData.mHeightField, sizeof(PhysX::HeightFieldT)))
                ShapeData.mType = PxGeometryType::eINVALID;
        }
        else if (ShapeData.mType == PxGeometryType::eCONVEXMESH)
        {
            PhysX::PxConvexMeshGeometryT convex_geometry = (PhysX::PxConvexMeshGeometryT&)ShapeData.Shape.GetGeometry();
            ShapeData.Scale = convex_geometry.Scale;
            if (!GetAddress((uint64_t)convex_geometry.ConvexMesh, &ShapeData.ConvexMesh, sizeof(PhysX::ConvexMeshT)))
                ShapeData.mType = PxGeometryType::eINVALID;
        }
    }

    for (auto& shapeData : ShapeDatas)
    {
        if (shapeData.mType == PxGeometryType::eCONVEXMESH)
        {
            auto nbPolygons = shapeData.ConvexMesh.HullData.PolygonsNb;
            auto mPolygons = (uint64_t)shapeData.ConvexMesh.HullData.mPolygons;
            if (!AddReadVecScatter(mPolygons, nbPolygons, &shapeData.polygons, 255))
                shapeData.mType = PxGeometryType::eINVALID;
        }
    }

    for (auto& ShapeData : ShapeDatas)
    {
        if (ShapeData.mType == PxGeometryType::eCONVEXMESH)
        {
            size_t indices_number = 0;
            if (!draw_only_scene_bounds::ConvexIndexCount(ShapeData.polygons, indices_number) ||
                !AddReadVecScatter<uint8_t>((uint64_t)ShapeData.ConvexMesh.HullData.getVertexData8(),
                                           indices_number, &ShapeData.ConvexIndices))
                ShapeData.mType = PxGeometryType::eINVALID;
        }
        else  if (ShapeData.mType == PxGeometryType::eTRIANGLEMESH)
        {
            auto& geometry = (PhysX::PxTriangleMeshGeometryT&)ShapeData.Shape.GetGeometry();
            const PhysX::PxTriangleMeshGeometryT& triangle_geometry = (PhysX::PxTriangleMeshGeometryT&)ShapeData.Shape.GetGeometry();
            ShapeData.Scale = triangle_geometry.Scale;
            if (!GetAddress((uintptr_t)triangle_geometry.mTriangleMesh, &ShapeData.TriangleMesh, sizeof(PhysX::TriangleMeshT)))
                ShapeData.mType = PxGeometryType::eINVALID;
        }
    }

    for (auto& ShapeData : ShapeDatas)
    {
        if (ShapeData.mType == PxGeometryType::eTRIANGLEMESH)
        {
            size_t indexCount = 0;
            if (!draw_only_scene_bounds::TriangleCounts(ShapeData.TriangleMesh.mNbVertices,
                    ShapeData.TriangleMesh.mNbTriangles, indexCount) ||
                !AddReadVecScatter<physx::PxVec3>((uint64_t)ShapeData.TriangleMesh.Vertices,
                    ShapeData.TriangleMesh.mNbVertices, &ShapeData.Vertices,
                    draw_only_scene_bounds::MaxVertices)) {
                ShapeData.mType = PxGeometryType::eINVALID;
                continue;
            }
            bool has16BitIndices = (ShapeData.TriangleMesh.Flags & 2U) ? true : false;
            if (has16BitIndices)
            {
                if (!AddReadVecScatter<uint16_t>((uint64_t)ShapeData.TriangleMesh.mTriangles, indexCount, &ShapeData.SmallIndices))
                    ShapeData.mType = PxGeometryType::eINVALID;
            }
            else
            {
                if (!AddReadVecScatter<uint32_t>((uint64_t)ShapeData.TriangleMesh.mTriangles, indexCount, &ShapeData.Indices))
                    ShapeData.mType = PxGeometryType::eINVALID;
            }
        }
        else if (ShapeData.mType == PxGeometryType::eHEIGHTFIELD)
        {
            const uint32_t nb = ShapeData.mHeightField.mNbSamples;
            draw_only_scene_bounds::HeightfieldLayout layout;
            if (!draw_only_scene_bounds::Heightfield(ShapeData.mHeightField.mData.Rows,
                    ShapeData.mHeightField.mData.Columns, nb, layout) ||
                !AddReadVecScatter((uint64_t)ShapeData.mHeightField.mData.mSamples, nb,
                    &ShapeData.mSamples, draw_only_scene_bounds::MaxSamples))
                ShapeData.mType = PxGeometryType::eINVALID;
        }
        else if (ShapeData.mType == PxGeometryType::eCONVEXMESH)
        {
            if (!AddReadVecScatter<physx::PxVec3>((uint64_t)ShapeData.ConvexMesh.HullData.getHullVertices(),
                    ShapeData.ConvexMesh.HullData.HullVerticesNb, &ShapeData.Vertices, 255))
                ShapeData.mType = PxGeometryType::eINVALID;
        }
    }

//#pragma omp parallel for
    for (int i = 0; i < ShapeDatas.size(); i++)
    {
        auto& ShapeData = ShapeDatas[i];
        if (ShapeData.mType == PxGeometryType::eTRIANGLEMESH && !ShapeData.SmallIndices.empty())
        {
//这个位置有点问题 会导致 0
            const size_t size = ShapeData.SmallIndices.size();
            ShapeData.Indices.resize(size);
            std::transform(
                    ShapeData.SmallIndices.begin(),
                    ShapeData.SmallIndices.end(),
                    ShapeData.Indices.begin(),
                    [](uint16_t val) { return static_cast<uint32_t>(val); }
            );
            ShapeData.SmallIndices.clear();
            ShapeData.SmallIndices.shrink_to_fit();
        }
    }

//#pragma omp parallel for

    for (int i = 0; i < ShapeDatas.size(); i++)
    {
        const auto& ShapeDataItem = ShapeDatas[i];
        if (ShapeDataItem.mType == PxGeometryType::eINVALID) continue;
        physx::PxTransform GlobalPose{};
        if (isDynamic) {
            GlobalPose = ShapeDataItem.Actor.mRigid.mCore.mBodyToWorld * ShapeDataItem.Actor.mRigid.mBodyToActor.getInverse();
        }
        else {
            GlobalPose = ShapeDataItem.Actor.mRigid.mCore.mBodyToWorld;
        }
        auto LocalPose = ShapeDataItem.Shape.ShapeCore.mCore.transform;
        auto CombinePose = GlobalPose * LocalPose;
        CombinePose.p = CombinePose.p/* + WorldLocation*/;//暂时不加
        auto UniqueKey = ShapeDataItem.UniqueKey;

        if (ShapeDataItem.mType == PxGeometryType::eBOX){
            const PhysX::PxBoxGeometry& boxGeometry = (PhysX::PxBoxGeometry&)ShapeDataItem.Shape.GetGeometry();
            auto halfExtents = boxGeometry.mHalfExtents;

            physx::PxVec3 vertices[8];
            vertices[0] = physx::PxVec3(-halfExtents.x, -halfExtents.y, -halfExtents.z);
            vertices[1] = physx::PxVec3( halfExtents.x, -halfExtents.y, -halfExtents.z);
            vertices[2] = physx::PxVec3( halfExtents.x,  halfExtents.y, -halfExtents.z);
            vertices[3] = physx::PxVec3(-halfExtents.x,  halfExtents.y, -halfExtents.z);
            vertices[4] = physx::PxVec3(-halfExtents.x, -halfExtents.y,  halfExtents.z);
            vertices[5] = physx::PxVec3( halfExtents.x, -halfExtents.y,  halfExtents.z);
            vertices[6] = physx::PxVec3( halfExtents.x,  halfExtents.y,  halfExtents.z);
            vertices[7] = physx::PxVec3(-halfExtents.x,  halfExtents.y,  halfExtents.z);

            if (autoTransform) {
                for (int i = 0; i < 8; i++)
                {
                    vertices[i] = CombinePose.transform(vertices[i]);
                    //LOGD("vertices[%d]:%2f\n",i,vertices[i].x);
                }
            }
            uint32_t indices[] = {
                    // 前脸 (Z负方向)
                    0, 1, 2,  0, 2, 3,
                    // 后脸 (Z正方向)
                    4, 6, 5,  4, 7, 6,  // 这里修正了：4,6,5 改为 4,5,6

                    // 上脸 (Y正方向)
                    3, 2, 6,  3, 6, 7,
                    // 下脸 (Y负方向)
                    0, 4, 5,  0, 5, 1,

                    // 右脸 (X正方向)
                    1, 5, 6,  1, 6, 2,
                    // 左脸 (X负方向)
                    0, 3, 7,  0, 7, 4
            };

            TriangleMeshData mesh_data{};
            mesh_data.Vertices.assign(vertices, vertices + 8);
            mesh_data.Indices.assign(indices, indices + 36);
            mesh_data.Flags = ShapeDataItem.m_shape_flags;
            mesh_data.QueryFilterData = ShapeDataItem.Shape.ShapeCore.QueryFilterData;
            mesh_data.SimulationFilterData = ShapeDataItem.Shape.ShapeCore.SimulationFilterData;
            mesh_data.UniqueKey1 = UniqueKey;
            mesh_data.Type = PxGeometryType::eBOX;
            mesh_data.Transform = CombinePose;

//#pragma omp critical
            {
                TriangleMeshDatas.push_back(mesh_data);
            }
        }
        else if (ShapeDataItem.mType == PxGeometryType::eCAPSULE)
        {
            const PhysX::PxCapsuleGeometryT& capsuleGeometry = (const PhysX::PxCapsuleGeometryT&)ShapeDataItem.Shape.GetGeometry();
            float radius = capsuleGeometry.mRadius;
            float halfHeight = capsuleGeometry.mHalfHeight;

        }
        else if (ShapeDataItem.mType == PxGeometryType::eSPHERE)
        {
            const PhysX::PxSphereGeometryT& sphereGeometry = (const PhysX::PxSphereGeometryT&)ShapeDataItem.Shape.GetGeometry();
            float radius = sphereGeometry.mRadius;
            const PhysX::PxMeshScale& ScaleVal = ShapeDataItem.Scale;

        }
        else if (ShapeDataItem.mType == PxGeometryType::eTRIANGLEMESH)
        {
            TriangleMeshData MeshData{};
            PhysX::PxMeshScale ScaleVal = ShapeDataItem.Scale;
            const size_t vertexCount = ShapeDataItem.Vertices.size();
            MeshData.Vertices.resize(vertexCount);
            for (size_t i = 0; i < vertexCount; i++)
            {
                physx::PxVec3& vertex = MeshData.Vertices[i];
                vertex = ScaleVal.transform(ShapeDataItem.Vertices[i]);
                if (autoTransform) {
                    vertex = CombinePose.transform(vertex);
                }
            }
            MeshData.Indices = ShapeDataItem.Indices;
            MeshData.Flags = ShapeDataItem.m_shape_flags;
            MeshData.QueryFilterData = ShapeDataItem.Shape.ShapeCore.QueryFilterData;
            MeshData.SimulationFilterData = ShapeDataItem.Shape.ShapeCore.SimulationFilterData;
            MeshData.UniqueKey1 = UniqueKey;
            MeshData.Type = PxGeometryType::eTRIANGLEMESH;
            MeshData.Transform = CombinePose;
//#pragma omp critical
            {
                TriangleMeshDatas.push_back(MeshData);
            }
        }
        else if (ShapeDataItem.mType == PxGeometryType::eHEIGHTFIELD)
        {
            PhysX::PxHeightFieldGeometryT field_geometry = (PhysX::PxHeightFieldGeometryT&)ShapeDataItem.Shape.GetGeometry();

            auto NumRows = ShapeDataItem.mHeightField.mData.Rows;
            auto NumColumns = ShapeDataItem.mHeightField.mData.Columns;
            auto ColumnScale = field_geometry.ColumnScale;
            auto RowScale = field_geometry.RowScale;
            auto HeightScale = field_geometry.HeightScale;

            TriangleMeshData MeshData{};

            draw_only_scene_bounds::HeightfieldLayout layout;
            if (!draw_only_scene_bounds::Heightfield(NumRows, NumColumns, ShapeDataItem.mSamples.size(), layout)) continue;
            MeshData.Vertices.reserve(layout.vertices);
            MeshData.Indices.reserve(layout.indices);
            for (size_t row = 0; row + 1 < layout.rows; row++) {
                for (size_t col = 0; col + 1 < layout.columns; col++) {
                    const size_t idx00 = row * layout.columns + col;
                    const size_t idx10 = idx00 + 1;
                    const size_t idx01 = idx00 + layout.columns;
                    const size_t idx11 = idx01 + 1;


                    auto sample00 = ShapeDataItem.mSamples[idx00];
                    auto sample10 = ShapeDataItem.mSamples[idx10];
                    auto sample01 = ShapeDataItem.mSamples[idx01];
                    auto sample11 = ShapeDataItem.mSamples[idx11];
                    physx::PxVec3 v00, v10, v01, v11;
                    v00 = { row * RowScale, sample00.mHeight * HeightScale, col * ColumnScale };
                    v10 = { row * RowScale, sample10.mHeight * HeightScale, (col + 1) * ColumnScale };
                    v01 = { (row + 1) * RowScale, sample01.mHeight * HeightScale, col * ColumnScale };
                    v11 = { (row + 1) * RowScale, sample11.mHeight * HeightScale, (col + 1) * ColumnScale };

                    if (autoTransform)
                    {
                        v00 = CombinePose.transform(v00);
                        v10 = CombinePose.transform(v10);
                        v01 = CombinePose.transform(v01);
                        v11 = CombinePose.transform(v11);
                    }

                    MeshData.Vertices.push_back(v00);
                    MeshData.Vertices.push_back(v10);
                    MeshData.Vertices.push_back(v01);
                    MeshData.Vertices.push_back(v11);

                    const uint32_t base_idx = static_cast<uint32_t>((row * (layout.columns - 1) + col) * 4);

                    MeshData.Indices.push_back(base_idx);
                    MeshData.Indices.push_back(base_idx + 1);
                    MeshData.Indices.push_back(base_idx + 2);

                    MeshData.Indices.push_back(base_idx + 1);
                    MeshData.Indices.push_back(base_idx + 3);
                    MeshData.Indices.push_back(base_idx + 2);
                }
            }

            MeshData.Flags = ShapeDataItem.m_shape_flags;
            MeshData.QueryFilterData = ShapeDataItem.Shape.ShapeCore.QueryFilterData;
            MeshData.SimulationFilterData = ShapeDataItem.Shape.ShapeCore.SimulationFilterData;
            MeshData.UniqueKey1 = UniqueKey;
            MeshData.UniqueKey2 = (uint64_t)((PhysX::PxHeightFieldGeometryT&)ShapeDataItem.Shape.GetGeometry()).mHeightField;
            MeshData.Type = PxGeometryType::eHEIGHTFIELD;
            MeshData.Transform = CombinePose;

//#pragma omp critical
            {
                TriangleMeshDatas.push_back(MeshData);
            }

        }
        else if (ShapeDataItem.mType == PxGeometryType::eCONVEXMESH)
        {
//凸包网格
//PxConvexMeshGeometryT convex_geometry = (PxConvexMeshGeometryT&)ShapeDataItem.Shape.GetGeometry();
            const PhysX::PxMeshScale ScaleVal = ShapeDataItem.Scale;
            const uint8_t nbVertices = ShapeDataItem.ConvexMesh.HullData.HullVerticesNb;
            const uint8_t nbPolygons = ShapeDataItem.ConvexMesh.HullData.PolygonsNb;
            bool validPolygons = !ShapeDataItem.polygons.empty();
            for (const auto& polygon : ShapeDataItem.polygons) {
                if (!draw_only_scene_bounds::PolygonRange(polygon.mVRef8, polygon.mNbVerts,
                                                         ShapeDataItem.ConvexIndices.size())) {
                    validPolygons = false; break;
                }
            }
            if (!validPolygons) continue;
            TriangleMeshData MeshData{};
            MeshData.Vertices = ShapeDataItem.Vertices;
            const size_t vertexCount = MeshData.Vertices.size();
            for (size_t i = 0; i < vertexCount; i++) {
//MeshData.Vertices[i] = ScaleVal.transform(MeshData.Vertices[i]);
                if (autoTransform) {
                    MeshData.Vertices[i] = CombinePose.transform(ScaleVal.transform(MeshData.Vertices[i]));
                }
            }
            for (const auto& polygon : ShapeDataItem.polygons) {
                for (uint16_t j = 0; j < polygon.mNbVerts - 2; j++) {
                    uint32_t index_idx0 = polygon.mVRef8 + 0;
                    uint32_t index_idx1 = polygon.mVRef8 + j + 1;
                    uint32_t index_idx2 = polygon.mVRef8 + j + 2;

                    uint32_t idx0 = ShapeDataItem.ConvexIndices[index_idx0];
                    uint32_t idx1 = ShapeDataItem.ConvexIndices[index_idx1];
                    uint32_t idx2 = ShapeDataItem.ConvexIndices[index_idx2];

                    MeshData.Indices.push_back(idx0);
                    MeshData.Indices.push_back(idx1);
                    MeshData.Indices.push_back(idx2);
                }
            }
            MeshData.Flags = ShapeDataItem.m_shape_flags;
            MeshData.QueryFilterData = ShapeDataItem.Shape.ShapeCore.QueryFilterData;
            MeshData.SimulationFilterData = ShapeDataItem.Shape.ShapeCore.SimulationFilterData;
            MeshData.UniqueKey1 = UniqueKey;
            MeshData.Type = PxGeometryType::eCONVEXMESH;
            MeshData.Transform = CombinePose;

//if (MeshData.QueryFilterData.word3 > 100) {
//#pragma omp critical
            {
                TriangleMeshDatas.push_back(MeshData);
            }
// }
        }
    }
/*
for (auto& meshData : TriangleMeshDatas) {
    MeshPatcher::GetInstance()->PatchMesh(meshData);
}
*/
    TriangleMeshDatas.erase(std::remove_if(TriangleMeshDatas.begin(), TriangleMeshDatas.end(),
        [](const TriangleMeshData& mesh) {
            return !draw_only_scene_bounds::MeshValid(mesh.Vertices, mesh.Indices);
        }), TriangleMeshDatas.end());
    if (report) for (const auto& mesh : TriangleMeshDatas) report->Built(mesh.UniqueKey1);
    return TriangleMeshDatas;
}

inline std::vector<PrunerPayload> CollectRigidActorShapes(uint16_t type, bool* complete) {
    if (complete) *complete = false;
    std::vector<PrunerPayload> result;
    uintptr_t headerAddress = 0;
    TArray<uint64_t> scenes{};
    if (!draw_only_scene_bounds::AddressOffset(PhysxInstancePtr.load(), 0x8, headerAddress) ||
        !GetAddress(headerAddress, &scenes, sizeof(scenes)) ||
        !draw_only_scene_bounds::ArrayValid(scenes.base, scenes.count, scenes.max,
            draw_only_scene_bounds::MaxScenes, sizeof(uint64_t))) return {};
    std::vector<uint64_t> scenePointers;
    if (!AddReadVecScatter(scenes.base, scenes.count, &scenePointers, draw_only_scene_bounds::MaxScenes)) return {};
    size_t totalActors = 0;
    for (const uint64_t scene : scenePointers) {
        TArray<uint64_t> actors{};
        if (!draw_only_scene_bounds::AddressOffset(scene, RigidActors, headerAddress) ||
            !GetAddress(headerAddress, &actors, sizeof(actors)) || !actors.IsValid()) return {};
        const auto count = static_cast<size_t>(actors.count);
        if (count > draw_only_scene_bounds::MaxActors - totalActors) return {};
        totalActors += count;
        std::vector<uint64_t> actorPointers;
        if (!AddReadVecScatter(actors.base, actors.count, &actorPointers, draw_only_scene_bounds::MaxActors)) return {};
        for (const uint64_t actor : actorPointers) {
            uintptr_t typeAddress = 0, shapeAddress = 0;
            if (!draw_only_scene_bounds::AddressOffset(actor, 0x8, typeAddress) ||
                !draw_only_scene_bounds::AddressOffset(actor, 0x28, shapeAddress)) return {};
            uint16_t actorType = 0;
            uint64_t shape = 0;
            if (!GetAddress(typeAddress, &actorType, sizeof(actorType))) return {};
            if (actorType != type) continue;
            if (!GetAddress(shapeAddress, &shape, sizeof(shape)) || shape < 0x1000) return {};
            result.push_back({.Shape = shape, .Actor = actor});
        }
    }
    if (complete) *complete = true;
    return result;
}
inline std::vector<PrunerPayload> CollectAllDynamicRigidActorShapes(bool* complete = nullptr) {
    return CollectRigidActorShapes(6, complete);
}
inline std::vector<PrunerPayload> CollectAllHeightRigidActorShapes(bool* complete = nullptr) {
    return CollectRigidActorShapes(7, complete);
}
inline std::vector<PrunerPayload> CollectAllStaticRigidActorShapes(bool* complete = nullptr) {
    return CollectRigidActorShapes(7, complete);
}

// Complete enumeration authorizes removals. A failed individual mesh read only
// leaves that object's previous state untouched; successful peers still commit.
inline std::vector<TriangleMeshData> LoadConfirmedRigidShapes(
        std::vector<PrunerPayload>& objects, bool dynamic,
        std::set<PrunerPayload>& currentSceneObjects,
        std::unordered_map<PrunerPayload, physx::PxTransform, PrunerPayloadHash>& cache,
        std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash>& revisions,
        std::set<PrunerPayload>& willRemoveObjects,
        draw_only_scene_bounds::LoadReport<PrunerPayload>& report) {
    std::unordered_map<PrunerPayload, physx::PxTransform, PrunerPayloadHash> nextPoses;
    std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash> nextRevisions;
    std::set<PrunerPayload> excluded;
    auto meshes = GetMeshData(objects, [&](const PhysX::ShapeDataT& data) {
        const auto key = data.UniqueKey;
        const auto pose = MeshPose(data, dynamic);
        const auto revision = MeshHeaderRevision(data);
        nextPoses.emplace(key, pose);
        nextRevisions.emplace(key, revision);
        const auto type = data.mType;
        const auto flags = data.m_shape_flags & 0xF;
        if (!((type == PxGeometryType::eTRIANGLEMESH || type == PxGeometryType::eBOX ||
               type == PxGeometryType::eCONVEXMESH) && (flags == 0x3 || flags == 0xB))) {
            excluded.insert(key);
            return true;
        }
        const auto previousPose = cache.find(key);
        const auto previousRevision = revisions.find(key);
        return currentSceneObjects.count(key) && previousPose != cache.end() &&
               previousRevision != revisions.end() && previousRevision->second == revision &&
               MeshPoseSame(previousPose->second, pose);
    }, dynamic, true, &report);

    const std::set<PrunerPayload> enumerated(objects.begin(), objects.end());
    for (auto it = currentSceneObjects.begin(); it != currentSceneObjects.end();) {
        if (!enumerated.count(*it)) {
            willRemoveObjects.insert(*it);
            it = currentSceneObjects.erase(it);
        } else ++it;
    }
    for (auto it = cache.begin(); it != cache.end();) {
        if (!enumerated.count(it->first)) it = cache.erase(it); else ++it;
    }
    for (auto it = revisions.begin(); it != revisions.end();) {
        if (!enumerated.count(it->first)) it = revisions.erase(it); else ++it;
    }
    const auto confirmMetadata = [&](const PrunerPayload& key) {
        cache[key] = nextPoses.at(key);
        revisions[key] = nextRevisions.at(key);
    };
    report.Commit([&](const PrunerPayload& key) {
        confirmMetadata(key);
        currentSceneObjects.insert(key);
        // UpdateMesh replaces this key directly. Failed replacements never
        // remove the old valid mesh while waiting for a future successful read.
    }, [&](const PrunerPayload& key) {
        if (excluded.count(key)) {
            confirmMetadata(key);
            if (currentSceneObjects.erase(key)) willRemoveObjects.insert(key);
        }
    });
    return meshes;
}

inline std::vector<TriangleMeshData> LoadDynamicRigidShape(
        std::set<PrunerPayload>& currentSceneObjects,
        std::unordered_map<PrunerPayload, physx::PxTransform, PrunerPayloadHash>& cache,
        std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash>& ptrCache,
        std::set<PrunerPayload>& willRemoveObjects) {
    bool complete = false;
    auto objects = CollectAllDynamicRigidActorShapes(&complete);
    if (!complete) return {};
    draw_only_scene_bounds::LoadReport<PrunerPayload> report;
    return LoadConfirmedRigidShapes(objects, true, currentSceneObjects, cache, ptrCache, willRemoveObjects, report);
}

inline bool ReadSceneTimestamp(uint32_t& timestamp) {
    uintptr_t header = 0;
    uint64_t array = 0, scenePointer = 0;
    PhysX::NpSceneT scene{};
    if (!draw_only_scene_bounds::AddressOffset(PhysxInstancePtr.load(), 0x8, header) ||
        !GetAddress(header, &array, sizeof(array)) ||
        !draw_only_scene_bounds::AddressRange(array, sizeof(scenePointer)) ||
        !GetAddress(array, &scenePointer, sizeof(scenePointer)) ||
        !draw_only_scene_bounds::AddressRange(scenePointer, sizeof(scene)) ||
        !GetAddress(scenePointer, &scene, sizeof(scene))) return false;
    timestamp = scene.exts[0].mTimestamp;
    return true;
}

inline std::vector<TriangleMeshData> RefreshDynamicLoadHeightField(
        uint32_t& lastTimestamp,
        std::set<PrunerPayload>& UniqueKeySet,
        std::set<PrunerPayload>& HeightFieldSet,
        std::set<uint64_t>& HeightFieldSamplePtrSet,
        std::set<uint64_t>& RemoveHeightFieldKey) {
    uint32_t timestamp = 0;
    if (!ReadSceneTimestamp(timestamp) || timestamp == lastTimestamp) return {};
    bool complete = false;
    auto objects = CollectAllHeightRigidActorShapes(&complete);
    if (!complete) return {};
    std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash> candidates;
    std::set<uint64_t> observedPointers, requestedPointers, builtPointers;
    draw_only_scene_bounds::LoadReport<PrunerPayload> report;
    auto meshes = GetMeshData(objects, [&](const PhysX::ShapeDataT& data) {
        if (data.mType != PxGeometryType::eHEIGHTFIELD) return true;
        const auto& geometry = (const PhysX::PxHeightFieldGeometryT&)data.Shape.GetGeometry();
        if (geometry.RowScale != 200.0f) return true;
        const uint64_t pointer = (uint64_t)geometry.mHeightField;
        candidates.emplace(data.UniqueKey, pointer);
        observedPointers.insert(pointer);
        // This set is local to the attempt. A failed first copy must never
        // make another shape sharing the pointer count as permanently loaded.
        return HeightFieldSamplePtrSet.count(pointer) || !requestedPointers.insert(pointer).second;
    }, false, true, &report);
    for (const auto& mesh : meshes) builtPointers.insert(mesh.UniqueKey2);
    // Resolve same-pointer duplicates only when one complete copy exists.
    report.RetryUnbuiltAliases(candidates, HeightFieldSamplePtrSet, builtPointers);
    const auto confirm = [&](const PrunerPayload& key) {
        UniqueKeySet.insert(key);
        if (candidates.count(key)) HeightFieldSet.insert(key); else HeightFieldSet.erase(key);
    };
    report.Commit(confirm, confirm);
    HeightFieldSamplePtrSet.insert(builtPointers.begin(), builtPointers.end());
    if (report.Complete()) {
        // Failed headers may hide an old pointer, so prune only a complete
        // attempt. Successfully built peers remain cached during retries.
        for (auto it = HeightFieldSamplePtrSet.begin(); it != HeightFieldSamplePtrSet.end();) {
            if (!observedPointers.count(*it)) {
                RemoveHeightFieldKey.insert(*it);
                it = HeightFieldSamplePtrSet.erase(it);
            } else ++it;
        }
        const std::set<PrunerPayload> enumerated(objects.begin(), objects.end());
        for (auto it = UniqueKeySet.begin(); it != UniqueKeySet.end();) {
            if (!enumerated.count(*it)) it = UniqueKeySet.erase(it); else ++it;
        }
        for (auto it = HeightFieldSet.begin(); it != HeightFieldSet.end();) {
            if (!enumerated.count(*it)) it = HeightFieldSet.erase(it); else ++it;
        }
    }
    report.AdvanceTimestamp(lastTimestamp, timestamp);
    return meshes;
}

inline std::vector<TriangleMeshData> LoadShapeByRange(
        uint32_t& lastTimestamp,
        std::unordered_map<PrunerPayload, physx::PxTransform, PrunerPayloadHash>& cache,
        std::set<PrunerPayload>& currentSceneObjects,
        std::set<PrunerPayload>& willRemoveObjects,
        std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash>& alwaysCheckShape) {
    uint32_t timestamp = 0;
    if (!ReadSceneTimestamp(timestamp) || timestamp == lastTimestamp) return {};
    bool complete = false;
    auto objects = CollectAllStaticRigidActorShapes(&complete);
    if (!complete) return {};
    draw_only_scene_bounds::LoadReport<PrunerPayload> report;
    auto meshes = LoadConfirmedRigidShapes(objects, false, currentSceneObjects, cache,
                                           alwaysCheckShape, willRemoveObjects, report);
    report.AdvanceTimestamp(lastTimestamp, timestamp);
    return meshes;
}

#include "rtcore.h"
#include "DrawOnlyVisibleScene.h"


namespace Physics {

    using namespace physx;
    using namespace std;

    static auto prunerPayloadExtractor = [](const TriangleMeshData& mesh) -> PrunerPayload {
        return mesh.UniqueKey1;
    };

    static auto int64Extractor = [](const TriangleMeshData& mesh) -> uint64_t {
        return mesh.UniqueKey2;
    };

    static auto normal64Extractor = [](const TriangleMeshData& mesh) -> uint64_t {
        return (uint64_t)rand();
    };

    template<class T, class Hash>
    using VisibleScene = draw_only_scene::VisibleScene<T, Hash, TriangleMeshData>;
} // namespace Physics

inline Physics::VisibleScene<PrunerPayload, PrunerPayloadHash>* DynamicLoadScene;
inline Physics::VisibleScene<PrunerPayload, PrunerPayloadHash>* DynamicRigidScene;
inline Physics::VisibleScene<uint64_t, Int64Hash>* HeightFieldScene;

/* ==================== 全局熔断闸 20261017 ====================
   场景：超体模式死亡→观战→复活跳伞→落地。这一个瞬间里
     · self_actor 被销毁重建
     · Actor 全表洗牌（cache / currentSceneObjects 全部失效）
     · 落地瞬间地形 & 建筑全量加载
   PhysX 三个扫描线程会同时爆发式全量重扫，把 RT 驱动 ioctl 通道占满，
   而绘制线程还要走同一个通道读内存 → 整体卡死。
   这里提供一个人人都能查的"闸门"，压力大时让三个线程主动让路。 */
namespace PhysXGate {
    /* C++17 inline 变量：无论多少个编译单元 include 本头，全程序只有一份。
       绘制.cpp 通过 extern 声明引用同一份，不需要 include 整个 PhysX.h。 */
    inline std::atomic<int> g_level{0};
    inline std::atomic<int> g_stress{0};
    inline std::atomic<int> g_spike{0};
    inline draw_only_physx::PressureCooldown g_recovery;

    inline std::atomic<int>& Level()  { return g_level; }
    inline std::atomic<int>& Stress() { return g_stress; }
    inline std::atomic<int>& Spike()  { return g_spike; }

    inline void MarkSpike() { g_spike.store(3); g_stress.store(0); }
    inline int  CurrentLevel() {
        if (g_spike.load() > 0) return 2;
        const int s = g_stress.load();
        return s >= 4 ? 2 : (s >= 2 ? 1 : 0);
    }
}


class Throttler {
public:
    Throttler();
    void executeTaskWithSleep(const std::string& taskName,std::chrono::duration<double> interval,const std::function<void()>& task);
private:
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastExecuted_;
    // Each worker owns one Throttler; these statistics must not be shared maps.
    std::unordered_map<std::string, double> lastCostMs;
    std::unordered_map<std::string, int> backoffLevel;
};
inline Throttler::Throttler() {}
/* 【对局卡死修复 20261017】
   原实现的问题：sleep 只保证"两轮之间至少隔 interval"，但 task() 本身耗时不受控。
   对局里对象从几十暴增到上千，LoadShapeByRange / LoadDynamicRigidShape 单轮
   可能跑几百毫秒甚至几秒 —— 这一轮里占满 RT 驱动的 ioctl 通道，
   绘制线程的读内存全被堵在后面 → 整个面板卡死、游戏也跟着卡。
   修复：① 记录上一轮 task() 实际耗时，下一轮至少 sleep(耗时×2)，给驱动通道留空隙；
         ② 单轮耗时超过 250ms 时，把间隔按倍数拉长（最多 8 倍），对局高峰期自动降频；
         ③ 两个线程各自的降频互相独立，互不影响。
   效果：漏打/画线用的 PhysX 场景数据照常有，只是刷新率在压力大时自动降低，
         不会因为"读不过来"把整个进程拖死。 */
inline void Throttler::executeTaskWithSleep(const std::string& taskName,std::chrono::duration<double> interval,const std::function<void()>& task)
{
    using Clock = std::chrono::steady_clock;
    // 每个 taskName 独立的耗时统计

    const double costMs = lastCostMs.count(taskName) ? lastCostMs[taskName] : 0.0;
    // 自适应退避：单轮耗时越长，间隔倍数越大（1x/2x/4x/8x，封顶 8x）
    int &lvl = backoffLevel[taskName];
    if (costMs > 250.0 && lvl < 3) lvl++;
    else if (costMs < 30.0 && lvl > 0) lvl--;
    const double scale = (double)(1 << lvl);

    auto now = Clock::now();
    {
        auto it = lastExecuted_.find(taskName);
        if (it != lastExecuted_.end()) {
            auto elapsed = now - it->second;
            // 目标间隔 = interval × 退避倍数
            auto target = std::chrono::duration_cast<Clock::duration>(interval * scale);
            if (elapsed < target) {
                std::this_thread::sleep_for(target - elapsed);
            }
        }
        // 额外保护：上一轮 task 耗时多久，就先让驱动通道歇同样久
        if (costMs > 0.0) {
            auto rest = std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double, std::milli>(costMs));
            std::this_thread::sleep_for(rest);
        }
        lastExecuted_[taskName] = Clock::now();
    }

    // 【熔断】场景突变（复活/落地）期间直接跳过本轮，别抢驱动通道
    if (PhysXGate::Spike().load() > 0) {
        PhysXGate::Spike().fetch_sub(1);
        lastExecuted_[taskName] = Clock::now();
        return;
    }
    // High pressure pauses bulk work, then admits one shared recovery probe.
    // A permanent early return would prevent task() from ever lowering stress.
    if (!PhysXGate::g_recovery.Allow(PhysXGate::Stress().load(),
                                    draw_only_physx::NowMilliseconds())) {
        lastExecuted_[taskName] = Clock::now();
        return;
    }

    auto t0 = Clock::now();
    task();
    auto t1 = Clock::now();
    const double cost = std::chrono::duration<double, std::milli>(t1 - t0).count();
    lastCostMs[taskName] = cost;

    draw_only_physx::ObserveTaskCost(PhysXGate::Stress(), cost);
}

namespace VisibleCheck {
    //按范围更新场景
    inline void UpdateSceneByRange() {
        std::unordered_map<PrunerPayload, physx::PxTransform, PrunerPayloadHash> cache{};
        std::set<PrunerPayload> currentSceneObjects{};
        std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash> alwaysCheckShape{};
        uint32_t lastUpdateTimestamp = 0;
        Throttler Throttlered;
        while (true) {
            Throttlered.executeTaskWithSleep("UpdateSceneByRangeSleep", std::chrono::milliseconds(800), [&cache, &currentSceneObjects, &lastUpdateTimestamp, &alwaysCheckShape] {
                std::set<PrunerPayload> willRemoveObjects{};
                auto Meshs = LoadShapeByRange(lastUpdateTimestamp,cache,currentSceneObjects,willRemoveObjects,alwaysCheckShape);
                if (!Meshs.empty() || !willRemoveObjects.empty()) DynamicLoadScene->UpdateMesh(Meshs, willRemoveObjects);
            });
        }
    }

    inline void UpdateDynamicHeightField() {
        std::set<PrunerPayload> UniqueSet{};
        std::set<PrunerPayload> HeightFieldSet{};
        std::set<uint64_t> HeightFieldSamplePtrSet{};
        uint32_t lastUpdateTimestamp = 0;
        Throttler Throttlered;
        while (true) {
            Throttlered.executeTaskWithSleep("HeightFieldUpdateSleep", std::chrono::milliseconds(1200), [&UniqueSet, &HeightFieldSet, &HeightFieldSamplePtrSet, &lastUpdateTimestamp] {
                std::set<uint64_t> RemoveHeightFieldKey{};
                auto Meshs = RefreshDynamicLoadHeightField(lastUpdateTimestamp, UniqueSet,HeightFieldSet,HeightFieldSamplePtrSet,RemoveHeightFieldKey);
                if (!Meshs.empty() || !RemoveHeightFieldKey.empty())HeightFieldScene->UpdateMesh(Meshs, RemoveHeightFieldKey);
            });
        }
    }

    inline void UpdateDynamicRigid() {
        Throttler Throttlered;
        std::unordered_map<PrunerPayload, physx::PxTransform, PrunerPayloadHash> cache{};
        std::unordered_map<PrunerPayload, uint64_t, PrunerPayloadHash> ptrCache{};
        std::set<PrunerPayload> currentSceneObjects{};
        while (true) {
            Throttlered.executeTaskWithSleep("DynamicRigidUpdateSleep", std::chrono::milliseconds(350), [&currentSceneObjects, &cache, &ptrCache] {
                std::set<PrunerPayload> willRemoveShape{};
                auto Meshs = LoadDynamicRigidShape(currentSceneObjects,cache,ptrCache,willRemoveShape);
                if (!Meshs.empty() || !willRemoveShape.empty()) DynamicRigidScene->UpdateMesh(Meshs, willRemoveShape);
            });
        }
    }

}


namespace LineTrace {
    static physx::PxVec3 ToPx(const FVector& v) {
        return {v.X, v.Y, v.Z};
    }
    static Vector3 ToVec3(const physx::PxVec3& v) {
        return {v.x, v.y, v.z};
    }
    inline draw_only_scene::RayStatus LineTraceSingle(FVector Location, FVector TraceEnd)
    {
        if (DynamicLoadScene == nullptr || HeightFieldScene == nullptr || DynamicRigidScene == nullptr) {
            return draw_only_scene::RayStatus::Invalid;
        }
        physx::PxVec3 origin = ToPx(Location);
        physx::PxVec3 target = ToPx(TraceEnd);
        auto status = DynamicLoadScene->TryRaycast(origin, target).status;
        if (status == draw_only_scene::RayStatus::Hit) return status;
        status = draw_only_scene::CombineRayStatus(status, HeightFieldScene->TryRaycast(origin, target).status);
        if (status == draw_only_scene::RayStatus::Hit) return status;
        return draw_only_scene::CombineRayStatus(status, DynamicRigidScene->TryRaycast(origin, target).status);
    }

    using MeshSnapshot = std::shared_ptr<const TriangleMeshData>;

    static MeshSnapshot getNextHint(FRotator& playerRotation, FVector& playerLocation,
                                    FVector* outHitWorld = nullptr) {
        if (outHitWorld) *outHitWorld = FVector{};
        if (!DynamicLoadScene || !HeightFieldScene || !DynamicRigidScene) return {};
        FVector forwardVector;
        forwardVector.X = cos(playerRotation.Yaw * M_PI / 180.0f) * cos(playerRotation.Pitch * M_PI / 180.0f);
        forwardVector.Y = sin(playerRotation.Yaw * M_PI / 180.0f) * cos(playerRotation.Pitch * M_PI / 180.0f);
        forwardVector.Z = sin(playerRotation.Pitch * M_PI / 180.0f);
        const physx::PxVec3 origin = ToPx(playerLocation);
        const physx::PxVec3 target = ToPx(playerLocation + forwardVector * 20000.0f);
        float minDistance = FLT_MAX;
        MeshSnapshot result;
        const auto choose = [&](const auto* scene) {
            MeshSnapshot candidate;
            const auto query = scene->TryRaycast(origin, target, &candidate);
            if (query.status == draw_only_scene::RayStatus::Busy ||
                query.status == draw_only_scene::RayStatus::Invalid) return false;
            const auto& hit = query.hit;
            if (candidate && hit.hit.geomID != RTC_INVALID_GEOMETRY_ID && hit.ray.tfar < minDistance) {
                minDistance = hit.ray.tfar;
                result = std::move(candidate);
            }
            return true;
        };
        // An incomplete scene could hide a nearer hit. Skip this frame's mesh
        // rather than block or display a potentially incorrect farther object.
        if (!choose(DynamicLoadScene) || !choose(HeightFieldScene) ||
            !choose(DynamicRigidScene)) return {};
        if (result && outHitWorld) {
            const float dx = target.x - origin.x, dy = target.y - origin.y, dz = target.z - origin.z;
            const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (length > 0.001f) {
                const float fraction = minDistance / length;
                outHitWorld->X = origin.x + dx * fraction;
                outHitWorld->Y = origin.y + dy * fraction;
                outHitWorld->Z = origin.z + dz * fraction;
            }
        }
        return result;
    }

    static MeshSnapshot getNextHintEx(FRotator& playerRotation, FVector& playerLocation,
                                      FVector* outHitWorld = nullptr) {
        // The hit point and mesh come from the same query and scene generation.
        return getNextHint(playerRotation, playerLocation, outHitWorld);
    }

}



#endif //PUBGM_PHYSX_H
