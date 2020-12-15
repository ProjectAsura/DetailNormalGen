#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>

static const uint64_t kMultiplier = 6364136223846793005u;
static const uint64_t kIncrement  = 1442695040888963407u;
uint64_t              g_State      = 0x4d595df4d0f33173;

uint32_t pcg_rotate(uint32_t x, uint32_t r)
{ return x >> r | x << ((~r + 1u) & 31); }

uint32_t pcg_u32()
{
    uint64_t x = g_State;
    uint32_t count = uint32_t(x >> 59);

    g_State = x * kMultiplier + kIncrement;
    x ^= x >> 18;
    return pcg_rotate(uint32_t(x >> 27), count);
}

float pcg_f32()
{
    return float(pcg_u32()) / float(UINT32_MAX);
}

void pcg32_init(uint64_t seed)
{
    g_State = seed + kIncrement;
    pcg_u32();
}

struct float3
{
    float x;
    float y;
    float z;
    float3(float nx, float ny, float nz)
        : x(nx), y(ny), z(nz)
    {}
};

float3 cross(const float3& a, const float3& b)
{
    return float3( 
        ( a.y * b.z ) - ( a.z * b.y ),
        ( a.z * b.x ) - ( a.x * b.z ),
        ( a.x * b.y ) - ( a.y * b.x ));
}

float3 normalize(const float3& v)
{
    auto mag = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return float3(v.x / mag, v.y / mag, v.z / mag);
}


enum TGA_COLORMAP_TYPE
{
    TGA_COLORMAP_DISABLE = 0,           // カラーマップなし.
    TGA_COLORMAP_ENABLE  = 1,           // カラーマップあり.
};

enum TGA_IMAGE_TYPE
{
    TGA_IMAGE_NO_IMAGE        = 0,      // イメージなし.
    TGA_IMAGE_INDEX_COLOR     = 1,      // インデックスカラー(256色).
    TGA_IMAGE_FULL_COLOR      = 2,      // フルカラー.
    TGA_IMAGE_MONOCHROME      = 3,      // 白黒.
    TGA_IMAGE_INDEX_COLOR_RLE = 9,      // インデックスカラーRLE圧縮.
    TGA_IMAGE_FULL_COLOR_RLE  = 10,     // フルカラーRLE圧縮.
    TGA_IMAGE_MONOCHROME_RLE  = 11      // 白黒RLE圧縮.
};

#pragma pack( push, 1 )
struct TGA_FILE_HEADER
{
    unsigned char       IdLength;           // IDフィールド (asdxではイメージID無し:0を設定).
    unsigned char       ColorMapType;       // カラーマップタイプ (asdxはカラーマップ無し:0を設定).
    unsigned char       ImageType;          // 画像タイプ.
    unsigned short      ColorMapIndex;      // カラーマップテーブルへのオフセット.
    unsigned short      ColorMapLength;     // カラーマップのエントリーの数.
    unsigned char       ColorMapSize;       // カラーマップの1ピクセルあたりのビット数.
    unsigned short      OriginX;            // 開始X位置.
    unsigned short      OriginY;            // 開始Y位置.
    unsigned short      Width;              // 画像の横幅.
    unsigned short      Height;             // 画像の縦幅.
    unsigned char       BitPerPixel;        // 1ピクセルあたりのビット数.
    unsigned char       Discripter;         // 記述子 [ bit0~3：属性, bit4:格納方向(0:左から右, 1:右から左), bit5, 格納方向(0:下から上, 1:上から下), bit6~7:0固定 ].
};
#pragma pack( pop )

#pragma pack( push, 1 )
struct TGA_FILE_FOOTER
{
    unsigned int        ExtOffset;      // 拡張エリアまでのオフセット (asdxは使わないので0固定).
    unsigned int        DevOffset;      // ディベロッパーエリアまでのオフセット (asdxは使わないので0固定).
    unsigned char       Magic[17];      // "TRUEVISION-XFILE" 固定
    unsigned char       RFU1;           // "." 固定.
    unsigned char       RFU2;           // 0 固定.
};
#pragma pack( pop )

void WriteTgaFileHeader( TGA_FILE_HEADER& header, FILE* pFile )
{
    fwrite( &header.IdLength,       sizeof(unsigned char),  1, pFile );
    fwrite( &header.ColorMapType,   sizeof(unsigned char),  1, pFile );
    fwrite( &header.ImageType,      sizeof(unsigned char),  1, pFile );
    fwrite( &header.ColorMapIndex,  sizeof(unsigned short), 1, pFile );
    fwrite( &header.ColorMapLength, sizeof(unsigned short), 1, pFile );
    fwrite( &header.ColorMapSize,   sizeof(unsigned char),  1, pFile );
    fwrite( &header.OriginX,        sizeof(unsigned short), 1, pFile );
    fwrite( &header.OriginY,        sizeof(unsigned short), 1, pFile );
    fwrite( &header.Width,          sizeof(unsigned short), 1, pFile );
    fwrite( &header.Height,         sizeof(unsigned short), 1, pFile );
    fwrite( &header.BitPerPixel,    sizeof(unsigned char),  1, pFile );
    fwrite( &header.Discripter,     sizeof(unsigned char),  1, pFile );
}

void WriteTgaFileFooter( TGA_FILE_FOOTER& footer, FILE* pFile )
{
    fwrite( &footer.ExtOffset,  sizeof(unsigned char), 1,   pFile );
    fwrite( &footer.DevOffset,  sizeof(unsigned char), 1,   pFile );
    fwrite( &footer.Magic,      sizeof(unsigned char), 17,  pFile );
    fwrite( &footer.RFU1,       sizeof(unsigned char), 1,   pFile );
    fwrite( &footer.RFU2,       sizeof(unsigned char), 1,   pFile );
}

void WriteTga( FILE* pFile, uint32_t width, uint32_t height, uint8_t bitPerPixel, const uint8_t* pixels )
{
    TGA_FILE_HEADER fileHeader = {};
    TGA_FILE_FOOTER fileFooter = {};

    fileHeader.ImageType = TGA_IMAGE_FULL_COLOR;
    fileHeader.Width = static_cast<unsigned short>( width );
    fileHeader.Height = static_cast<unsigned short>( height );
    fileHeader.BitPerPixel = bitPerPixel;

    unsigned char magic[17] = "TRUEVISION-XFILE";
    memcpy( fileFooter.Magic, magic, sizeof(unsigned char) * 17 );
    fileFooter.RFU1 = '.';

    int bytePerPixel = fileHeader.BitPerPixel / 8;

    WriteTgaFileHeader( fileHeader, pFile );
    for ( int i=height -1; i>=0; i-- )
    {
        for( int j=0; j<width; ++j )
        {
            int index = ( i * width * bytePerPixel ) + j * bytePerPixel;

            if ( bytePerPixel == 4 )
            {
                fwrite( &pixels[index + 2], sizeof(unsigned char), 1, pFile );
                fwrite( &pixels[index + 1], sizeof(unsigned char), 1, pFile );
                fwrite( &pixels[index + 0], sizeof(unsigned char), 1, pFile );
                fwrite( &pixels[index + 3], sizeof(unsigned char), 1, pFile );
            }
            else if ( bytePerPixel == 3 )
            {
                fwrite( &pixels[index + 2], sizeof(unsigned char), 1, pFile );
                fwrite( &pixels[index + 1], sizeof(unsigned char), 1, pFile );
                fwrite( &pixels[index + 0], sizeof(unsigned char), 1, pFile );
            }
        }
    }
    WriteTgaFileFooter( fileFooter, pFile );
}

bool SaveTextureToTgaA
(
    const char*     filename,
    uint32_t        width,
    uint32_t        height,
    uint8_t         component, // 3 or 4.
    const uint8_t*  pPixels
)
{
    FILE* pFile;
    errno_t err = fopen_s( &pFile, filename, "wb" );
    if ( err != 0 )
    {
        return false;
    }

    WriteTga( pFile, width, height, component * 8, pPixels);
    fclose( pFile );

    return true;
}

int idx(int x, int y, int s)
{
    if (x < 0)
    { x += s; }
    if (y < 0)
    { y += s; }

    if (x > (s - 1))
    { x = x % s; }

    if (y > (s - 1))
    { y = y % s; }

    auto index = x + y * s;
    assert(index <= (s * s));
    return index;
}

float h(int x, int y, int s, const std::vector<float>& pixels)
{
    return pixels.at(idx(x, y, s));
}


int main(int argc, char** argv)
{
    int kSize = 1024;

    pcg32_init(123456789);

    const int kCount = 100;
    float rx[kCount] = {};
    float ry[kCount] = {};
    float frequency[kCount] = {};

    for(auto i=0; i<kCount; ++i)
    {
        rx[i]        = pcg_f32() * kSize;
        ry[i]        = pcg_f32() * kSize;
        frequency[i] = pcg_f32() * kSize;
    }

    std::vector<float> heights;
    {
        size_t size = kSize;
        size *= kSize;
        heights.resize(size);
    }

    // ハイトマップ生成.
    for(size_t i=0; i<kSize; ++i)
    {
        for(size_t j=0; j<kSize; ++j)
        {
            auto z = 0.0f;
            for(auto k=0; k<kCount; ++k)
            {
                auto x = float(j) - rx[k];
                auto y = float(i) - ry[k];
                z += sin( (x * x + y * y) / (2.08f + 5.0f * frequency[k]) );
            }
            z /= float(kCount);

            auto idx = j + i * kSize;
            heights[idx] = z;
        }
    }

    std::vector<uint8_t> normals;
    {
        size_t size = kSize;
        size *= kSize;
        size *= 4;
        normals.resize(size);
    }

    for(auto i=0; i<kSize; ++i)
    {
        for(auto j=0; j<kSize; ++j)
        {
            auto du = normalize(float3(1.0f, 0.0f, (h(j+1, i, kSize, heights) - h(j-1, i, kSize, heights)) * 0.5f));
            auto dv = normalize(float3(0.0f, 1.0f, (h(j, i+1, kSize, heights) - h(j, i-1, kSize, heights)) * 0.5f));

            auto n = normalize(cross(du, dv));

            auto idx = j * 4 + i * kSize * 4;
            normals[idx + 0] = uint8_t((n.x * 0.5f + 0.5f) * 255);
            normals[idx + 1] = uint8_t((n.y * 0.5f + 0.5f) * 255);
            normals[idx + 2] = uint8_t((n.z * 0.5f + 0.5f) * 255);
            normals[idx + 3] = uint8_t((h(j, i, kSize, heights) * 0.5f + 0.5f) * 255);
        }
    }

    SaveTextureToTgaA("detail_normal.tga", kSize, kSize, 4, normals.data());
    heights.clear();
    normals.clear();

    return 1;
}