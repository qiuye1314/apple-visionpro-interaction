#include "辅助类.h"
#include "map"
#include "DrawOnlyBoneReader.h"

class 骨骼
{
public:
  std::unordered_map<std::string, std::string> BossName;
  Kernel *读写;

  骨骼(Kernel *读写) : 读写(读写)
  {
    BossName = {
        {"Pawn_Escape_RD_Grenade_C", "巡卫长·玄铁"},
        {"Pawn_Escape_RD_RoyalGuards_C", "影卫·银星"},
        {"Pawn_Escape_Boss_Robocop_C", "V-34机械警"},
        {"Pawn_Escape_BOSS_Claws_C", "钢爪·安德烈"},
        {"Pawn_Escape_RD_SupplyBoss_C", "辎重使·墨守"},
    };
  }

  bool isBoss(const char* className) {
    if (!className) return false;
    for (const auto& item : BossName) if (strstr(className, item.first.c_str())) return true;
    return false;
  }

  bool getBossName(const char* className, std::string& name) {
    if (!className) return false;
    for (const auto& item : BossName) {
      if (strstr(className, item.first.c_str())) { name = item.second; return true; }
    }
    return false;
  }

  FTransform getBone(uintptr_t addr)
  {
    FTransform transform{};
    draw_only_bones::ReadTransform(addr, transform,
      [this](uintptr_t address, void* buffer, size_t size) {
        return 读写 && 读写->readv(address, buffer, size);
      });
    return transform;
  }

  D3DVector MarixToVector(FMatrix matrix)
  {
    return D3DVector{matrix.M[3][0], matrix.M[3][1], matrix.M[3][2]};
  }

  FMatrix MatrixMulti(const FMatrix &m1, const FMatrix &m2)
  {
    FMatrix matrix;
    for (int i = 0; i < 4; i++)
    {
      for (int j = 0; j < 4; j++)
      {
        matrix.M[i][j] = 0;
        for (int k = 0; k < 4; k++)
        {
          matrix.M[i][j] += m1.M[i][k] * m2.M[k][j];
        }
      }
    }
    return matrix;
  }

  FMatrix TransformToMatrix(FTransform transform)
  {
    FMatrix matrix;
    matrix.M[3][0] = transform.Translation.X;
    matrix.M[3][1] = transform.Translation.Y;
    matrix.M[3][2] = transform.Translation.Z;
    float x2 = transform.Rotation.X + transform.Rotation.X;
    float y2 = transform.Rotation.Y + transform.Rotation.Y;
    float z2 = transform.Rotation.Z + transform.Rotation.Z;
    float xx2 = transform.Rotation.X * x2;
    float yy2 = transform.Rotation.Y * y2;
    float zz2 = transform.Rotation.Z * z2;
    matrix.M[0][0] = (1 - (yy2 + zz2)) * transform.Scale3D.X;
    matrix.M[1][1] = (1 - (xx2 + zz2)) * transform.Scale3D.Y;
    matrix.M[2][2] = (1 - (xx2 + yy2)) * transform.Scale3D.Z;
    float yz2 = transform.Rotation.Y * z2;
    float wx2 = transform.Rotation.W * x2;
    matrix.M[2][1] = (yz2 - wx2) * transform.Scale3D.Z;
    matrix.M[1][2] = (yz2 + wx2) * transform.Scale3D.Y;
    float xy2 = transform.Rotation.X * y2;
    float wz2 = transform.Rotation.W * z2;
    matrix.M[1][0] = (xy2 - wz2) * transform.Scale3D.Y;
    matrix.M[0][1] = (xy2 + wz2) * transform.Scale3D.X;
    float xz2 = transform.Rotation.X * z2;
    float wy2 = transform.Rotation.W * y2;
    matrix.M[2][0] = (xz2 + wy2) * transform.Scale3D.Z;
    matrix.M[0][2] = (xz2 - wy2) * transform.Scale3D.X;
    matrix.M[0][3] = 0;
    matrix.M[1][3] = 0;
    matrix.M[2][3] = 0;
    matrix.M[3][3] = 1;
    return matrix;
  }

  D3DXMATRIX ToMatrixWithScale(D3DXVECTOR4 Rotation, D3DVector Translation, D3DVector Scale3D)
  {
    D3DXMATRIX M;
    float X2, Y2, Z2, xX2, Yy2, Zz2, Zy2, Wx2, Xy2, Wz2, Zx2, Wy2;
    M._41 = Translation.X;
    M._42 = Translation.Y;
    M._43 = Translation.Z;
    X2 = Rotation.X + Rotation.X;
    Y2 = Rotation.Y + Rotation.Y;
    Z2 = Rotation.Z + Rotation.Z;
    xX2 = Rotation.X * X2;
    Yy2 = Rotation.Y * Y2;
    Zz2 = Rotation.Z * Z2;
    M._11 = (1 - (Yy2 + Zz2)) * Scale3D.X;
    M._22 = (1 - (xX2 + Zz2)) * Scale3D.Y;
    M._33 = (1 - (xX2 + Yy2)) * Scale3D.Z;
    Zy2 = Rotation.Y * Z2;
    Wx2 = Rotation.W * X2;
    M._32 = (Zy2 - Wx2) * Scale3D.Z;
    M._23 = (Zy2 + Wx2) * Scale3D.Y;
    Xy2 = Rotation.X * Y2;
    Wz2 = Rotation.W * Z2;
    M._21 = (Xy2 - Wz2) * Scale3D.Y;
    M._12 = (Xy2 + Wz2) * Scale3D.X;
    Zx2 = Rotation.X * Z2;
    Wy2 = Rotation.W * Y2;
    M._31 = (Zx2 + Wy2) * Scale3D.Z;
    M._13 = (Zx2 - Wy2) * Scale3D.X;
    M._14 = 0;
    M._24 = 0;
    M._34 = 0;
    M._44 = 1;
    return M;
  }

  FTransform1 ReadFTransform(long int address)
  {
    FTransform1 Result;
    Result.Rotation.X = 读写->getFloat(address);         // Rotation_X
    Result.Rotation.Y = 读写->getFloat(address + 4);     // Rotation_y
    Result.Rotation.Z = 读写->getFloat(address + 8);     // Rotation_z
    Result.Rotation.W = 读写->getFloat(address + 12);    // Rotation_w
    Result.Translation.X = 读写->getFloat(address + 16); // /Translation_X
    Result.Translation.Y = 读写->getFloat(address + 20); // Translation_y
    Result.Translation.Z = 读写->getFloat(address + 24); // Translation_z
    Result.Scale3D.X = 读写->getFloat(address + 32);
    ; // Scale_X
    Result.Scale3D.Y = 读写->getFloat(address + 36);
    ; // Scale_y
    Result.Scale3D.Z = 读写->getFloat(address + 40);
    ; // Scale_z
    return Result;
  }

  // 获取骨骼3d坐标
  D3DVector D3dMatrixMultiply(D3DXMATRIX bonematrix, D3DXMATRIX actormatrix)
  {
    D3DVector result;
    result.X =
        bonematrix._41 * actormatrix._11 + bonematrix._42 * actormatrix._21 +
        bonematrix._43 * actormatrix._31 + bonematrix._44 * actormatrix._41;
    result.Y =
        bonematrix._41 * actormatrix._12 + bonematrix._42 * actormatrix._22 +
        bonematrix._43 * actormatrix._32 + bonematrix._44 * actormatrix._42;
    result.Z =
        bonematrix._41 * actormatrix._13 + bonematrix._42 * actormatrix._23 +
        bonematrix._43 * actormatrix._33 + bonematrix._44 * actormatrix._43;
    return result;
  }

  D3DVector getBoneXYZ(long int humanAddr, long int boneAddr, int Part)
  {
    // 获取Bone数据
    FTransform1 Bone = ReadFTransform(boneAddr + Part * 48);
    // 获取Actor数据
    FTransform1 Actor = ReadFTransform(humanAddr);
    D3DXMATRIX Bone_Matrix = ToMatrixWithScale(Bone.Rotation, Bone.Translation, Bone.Scale3D);
    D3DXMATRIX Component_ToWorld_Matrix =
        ToMatrixWithScale(Actor.Rotation, Actor.Translation, Actor.Scale3D);
    D3DVector result = D3dMatrixMultiply(Bone_Matrix, Component_ToWorld_Matrix);
    return result;
  }



  void 更新骨骼数据(uintptr_t MeshAddress, uintptr_t Bone, D3DVector (&骨骼坐标)[17], int Bonecount, int Team, char *类名, D3DVector 坐标)
  {
    for (auto& point : 骨骼坐标) point = {};
    if (!读写 || !类名) return;
    static_assert(sizeof(FTransform) == 44, "FTransform payload must match the 48-byte array stride");
    static_assert(offsetof(FTransform, Translation) == 16 && offsetof(FTransform, Scale3D) == 32,
                  "FTransform field offsets must match the source layout");
    (void)Team;
    (void)坐标;
    auto rig = draw_only_bones::Rig::Standard;
    if (isBoss(类名)) {
      std::string name;
      getBossName(类名, name);
      rig = name == "钢爪·安德烈" ? draw_only_bones::Rig::ClawsBoss : draw_only_bones::Rig::GuardBoss;
    }
    // Preserve component translation/rotation; actor root and animated mesh need not coincide.
    // Bone is already array + 0x30, so the helper checks actual element index + 1.
    draw_only_bones::ReadSkeleton<FTransform>(MeshAddress, Bone, Bonecount,
      draw_only_bones::Mapping(Bonecount, rig), 骨骼坐标,
      [this](uintptr_t address, void* buffer, size_t size) {
        return 读写->readv(address, buffer, size);
      });
  }
};
