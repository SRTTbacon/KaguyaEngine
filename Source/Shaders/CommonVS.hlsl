cbuffer CommonVSBuffer : register(b0, space1)
{
    matrix viewMatrix;          // カメラのビューマトリックス
    matrix viewMatrixPrev;      // 前フレームのカメラのビューマトリックス
    matrix projectionMatrix;    // カメラのプロジェクションマトリックス
    matrix projectionMatrixPrev;// 前フレームのカメラのプロジェクションマトリックス
    matrix lightViewProj;       // ライトのビュープロジェクション
    float4 lightPos;            // ライトの位置
    float4 cameraPos;           // カメラの位置
    float4 cameraJitters;       // カメラのジャイター情報
    float time;                 // 時間情報
    float3 padding;             // 16バイト境界に合わせるためのパディング
};

// 必須固有バッファ
struct UniqueVSStruct
{
    matrix modelMatrix;         //モデル全体のマトリックス
    matrix modelMatrixPrev;     //前のフレームのモデル全体のマトリックス
    matrix meshMatrix;          //メッシュ単位のマトリックス
    matrix meshMatrixPrev;      //前のフレームのメッシュ単位のマトリックス
    matrix normalMatrix;        //モデルのスケール、回転などをinput.normalにも適応する用
};

// 独自にUniquePSBufferを定義する場合はinclude前に
// #define COMMON_BUFFER_DEFINED を宣言してください。
#ifndef COMMON_BUFFER_DEFINED
#define COMMON_BUFFER_DEFINED
cbuffer UniqueVSBuffer : register(b1, space1)
{
    UniqueVSStruct commonBuffer;
};
#endif

#ifndef VSINPUT_DEFINED
#define VSINPUT_DEFINED
struct VSInput
{
    float3 pos : POSITION;              // 頂点座標
    float3 normal : NORMAL;             // 法線
    float2 uv : TEXCOORD;               // UV
    float3 tangent : TANGENT;           // 接線
    float3 binormal : BINORMAL;         // 従法線
    uint vertexID : VERTEXID;           // 頂点のID
};
#endif

#ifndef VSOUTPUT_DEFINE
#define VSOUTPUT_DEFINE
struct VSOutput
{
    float4 svpos : SV_POSITION;     //スクリーン座標
    float3 normal : NORMAL;         //法線
    float2 uv : TEXCOORD;           //UV
    float3 worldPos : TEXCOORD1;    //ワールド座標
    float4 projPos : TEXCOORD2;
    float4 projPosPrev : TEXCOORD3;
    float viewPosZ : TEXCOORD4;
};
#endif
