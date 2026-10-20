//
// Render
//
// �?C++ 版（x64 可用，已移除全部内联 __asm�?
//
// 原实现里 Render16bpp / Render16bppPrefilter / Render32bpp 三个函数是手�?__asm�?
// 这里改为等价 C++。原本的汇编语义�?
//   1. 一次读 4 字节源数�?= 4 个调色板索引（小端：dl 是第 1 个像素）
//   2. 每个索引�?32 位调色板 m_?nPalette[mode&7][idx]（[esi+ecx*4]，DWORD 步进�?
//   3. 按目标色深截断写出：16bpp 取低 16 位�?2bpp 取整 DWORD
//   4. 逐行 lpSrc += RENDER_WIDTH，lpDst += SCREEN_WIDTH*bpp
//
// 本文件顶部的 vnesx:: 辅助层是 8/16/24/32bpp 四个头文件的公共基础�?
// 必须�?#include "RenderXXbpp.h" 之前（即本文件里已经定义好了）�?
//

#include <string.h>				// memcpy

#if defined(_MSC_VER)
#  define VNES_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#  define VNES_RESTRICT __restrict__
#else
#  define VNES_RESTRICT
#endif

//==================================================================
// 公共辅助层：像素读写 + 单行渲染内核
//==================================================================
namespace vnesx {

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

// 与汇�?mov / shr 等价的小端读写�?
// �?memcpy 避开对齐�?strict-aliasing 问题，x86/x64 上编译器直接折叠成一�?mov�?
inline u32  load32( const void* p )   { u32 v; memcpy( &v, p, 4 ); return v; }
inline void store32( void* p, u32 v ) { memcpy( p, &v, 4 ); }
inline void store16( void* p, u16 v ) { memcpy( p, &v, 2 ); }

//------------------------------------------------------------------
// 像素写出器：bytes = 目标色深字节数，xscale = 水平放大倍数
//------------------------------------------------------------------
struct Put16x1 { enum { bytes = 2, xscale = 1 };
	static inline void put( u8* p, u32 c ) { store16( p, (u16)c ); } };

struct Put16x2 { enum { bytes = 2, xscale = 2 };
	static inline void put( u8* p, u32 c ) { u16 v = (u16)c; store16( p, v ); store16( p+2, v ); } };

struct Put24x1 { enum { bytes = 3, xscale = 1 };
	static inline void put( u8* p, u32 c ) { p[0] = (u8)c; p[1] = (u8)(c>>8); p[2] = (u8)(c>>16); } };

struct Put24x2 { enum { bytes = 3, xscale = 2 };
	static inline void put( u8* p, u32 c ) {
		p[0] = (u8)c; p[1] = (u8)(c>>8); p[2] = (u8)(c>>16);
		p[3] = (u8)c; p[4] = (u8)(c>>8); p[5] = (u8)(c>>16);
	} };

struct Put32x1 { enum { bytes = 4, xscale = 1 };
	static inline void put( u8* p, u32 c ) { store32( p, c ); } };

struct Put32x2 { enum { bytes = 4, xscale = 2 };
	static inline void put( u8* p, u32 c ) { store32( p, c ); store32( p+4, c ); } };

//------------------------------------------------------------------
// 单行渲染（对应原实现里的一�?__asm 块）
//   src  : 本行源数据（8bit 索引�?
//   dst  : 目标行首地址
//   pal  : 本行调色板（256 �?DWORD�?
//   dlt  : 本行 delta 数据首地址（SCREEN_WIDTH 字节�?
//   fw   : 强制整行重写（colormode 变了或外部要求）
//   wdlt : 是否回写 delta
//          —�?两遍模式（Scanline/Double/DoubleScanline）下只有【最后一遍】为 true�?
//             对应汇编注释�?回目は書き込まな�?/ 2回目は書き込む」；
//             否则第二遍会因为 delta 已等于新值而整行跳过�?
//------------------------------------------------------------------
template<class W>
static inline void Row( const u8*  VNES_RESTRICT src,
                        u8*        VNES_RESTRICT dst,
                        const u32* VNES_RESTRICT pal,
                        int width, u8* dlt, bool fw, bool wdlt )
{
	const int step = (int)W::bytes * (int)W::xscale;	// 每个源像素占的目标字节数
	int x = 0;

	for( ; x + 4 <= width; x += 4 ) {
		const u32 v = load32( src + x );
		u8* dg = dlt + x;								// 汇编: cmp edx, [ebx]
		if( !fw ) {
			if( load32( dg ) == v )						// 与上一帧相�?-> 整组跳过
				continue;
			if( wdlt ) store32( dg, v );				// mov [ebx+0], edx
		} else {
			if( wdlt ) store32( dg, v );
		}
		W::put( dst + (x+0)*step, pal[ (v      ) & 0xFF ] );
		W::put( dst + (x+1)*step, pal[ (v >>  8) & 0xFF ] );
		W::put( dst + (x+2)*step, pal[ (v >> 16) & 0xFF ] );
		W::put( dst + (x+3)*step, pal[ (v >> 24) & 0xFF ] );
	}
	// SCREEN_WIDTH �?4 的倍数�?56）时不会走到这里，仅作兜�?
	for( ; x < width; ++x ) {
		const u32 v = src[x];
		if( !fw ) {
			if( dlt[x] == v ) continue;
			if( wdlt ) dlt[x] = (u8)v;
		} else {
			if( wdlt ) dlt[x] = (u8)v;
		}
		W::put( dst + x*step, pal[v] );
	}
}

// 8bit 调色板模式：索引直接写出，扫描线�?OR 0x40 / 0x80 切到另一段逻辑调色�?
//（对应汇编的 or eax,0x40404040 �?MMX �?punpcklbw + por 0x8080...�?
static inline void Row8( const u8* VNES_RESTRICT src, u8* VNES_RESTRICT dst,
                         int width, u8 bias, int xscale )
{
	if( xscale == 1 ) {
		for( int x = 0; x < width; ++x )
			dst[x] = (u8)( src[x] | bias );
	} else {
		for( int x = 0; x < width; ++x ) {
			const u8 v = (u8)( src[x] | bias );
			dst[2*x  ] = v;								// punpcklbw 的效�?
			dst[2*x+1] = v;
		}
	}
}

} // namespace vnesx

//==================================================================
// 16bit Render（滤镜预渲染用，�?16bpp 中间缓冲�?
//==================================================================
void	CDirectDraw::Render16bpp( LPBYTE lpSrc, LPBYTE lpDst )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;

	for( int i = 0; i < H; i++ ) {
		const int cm = m_LineColormode[i];
		const vnesx::u32* pal = ( cm & 0x80 )
			? (const vnesx::u32*)m_mnPalette[cm & 0x07]
			: (const vnesx::u32*)m_cnPalette[cm & 0x07];

		const vnesx::u8* s = (const vnesx::u8*)lpSrc + i*RENDER_WIDTH;
		vnesx::u8*       d = (vnesx::u8*)lpDst + i*W*sizeof(WORD);

		for( int x = 0; x < W; x++ )
			vnesx::store16( d + x*2, (vnesx::u16)pal[s[x]] );
	}
}

//==================================================================
// 16bit Pre-Render（带前置滤波的调色板，供 32bpp 滤镜链使用）
//==================================================================
void	CDirectDraw::Render16bppPrefilter( LPBYTE lpSrc, LPBYTE lpDst )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;

	for( int i = 0; i < H; i++ ) {
		const int cm = m_LineColormode[i];
		const vnesx::u32* pal = ( cm & 0x80 )
			? (const vnesx::u32*)m_mfPalette[cm & 0x07]
			: (const vnesx::u32*)m_cfPalette[cm & 0x07];

		const vnesx::u8* s = (const vnesx::u8*)lpSrc + i*RENDER_WIDTH;
		vnesx::u8*       d = (vnesx::u8*)lpDst + i*W*sizeof(WORD);

		for( int x = 0; x < W; x++ )
			vnesx::store16( d + x*2, (vnesx::u16)pal[s[x]] );
	}
}

//==================================================================
// 32bit Render
//==================================================================
void	CDirectDraw::Render32bpp( LPBYTE lpSrc, LPBYTE lpDst )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;

	for( int i = 0; i < H; i++ ) {
		const int cm = m_LineColormode[i];
		const vnesx::u32* pal = ( cm & 0x80 )
			? (const vnesx::u32*)m_mnPalette[cm & 0x07]
			: (const vnesx::u32*)m_cnPalette[cm & 0x07];

		const vnesx::u8* s = (const vnesx::u8*)lpSrc + i*RENDER_WIDTH;
		vnesx::u8*       d = (vnesx::u8*)lpDst + i*W*sizeof(DWORD);

		for( int x = 0; x < W; x++ )
			vnesx::store32( d + x*4, pal[s[x]] );
	}
}

// Normal/Scanline/Double/DoubleScanlie Render
#include "Render8bpp.h"
#include "Render16bpp.h"
#include "Render24bpp.h"
#include "Render32bpp.h"

typedef	signed char		eI8;
typedef	signed short		eI16;
typedef	signed long		eI32;
typedef	signed __int64		eI64;
typedef	unsigned char		euI8;
typedef	unsigned short		euI16;
typedef	unsigned long		euI32;
typedef	unsigned __int64	euI64;

static	euI64	ONE   = 0x0001000100010001;
static	euI64	cMask;
static	euI64	qMask;
static	euI64	lMask;
static	euI64	ACPixel;
static	euI64	Mask1;
static	euI64	Mask2;
static	euI64	I56Pixel;
static	euI64	I5556Pixel;
static	euI64	I5666Pixel;
static	euI64	I23Pixel;
static	euI64	I2223Pixel;
static	euI64	I2333Pixel;
static	euI64	Mask26;
static	euI64	Mask35;
static	euI64	Mask26b;
static	euI64	Mask35b;
static	euI64	product1a;
static	euI64	product1b;
static	euI64	product2a;
static	euI64	product2b;
static	euI64	final1a;
static	euI64	final1b;
static	euI64	final2a;
static	euI64	final2b;

#define	colorI		-2
#define	colorE		0
#define	colorF		2
#define	colorJ		4
#define	colorG		-2
#define	colorA		0
#define	colorB		2
#define	colorK		4
#define	colorH		-2
#define	colorC		0
#define	colorD		2
#define	colorL		4
#define	colorM		-2
#define	colorN		0
#define	colorO		2
#define	colorP		4

#define	colorB0		-2
#define	colorB1		0
#define	colorB2		2
#define	colorB3		4
#define	color4		-2
#define	color5		0
#define	color6		2
#define	colorS2		4
#define	color1		-2
#define	color2		0
#define	color3		2
#define	colorS1		4
#define	colorA0		-2
#define	colorA1		0
#define	colorA2		2
#define	colorA3		4

//------------------------------------------------------------------
// 滤镜部分
//
// nx_*.h 里的 *_mmx() 内核本身也是 MMX 内联汇编，x64 下同样编译不过�?
// �?x86（即 x64）时自动启用 VNES_X64_NO_MMX：所�?nx_* 滤镜降级�?
// 对应�?Double / DoubleScanline 放大，保证工程先能编译链接、画面正常�?
//
// 想把滤镜真正移植过来（它们本体是 C 逻辑 + MMX 加速，�?C �?SSE2 都不难）�?
// 移植�?*_mmx() 后，�?x64 配置里定�?VNES_KEEP_MMX_FILTERS 即可恢复原路径�?
//------------------------------------------------------------------
#if !defined(_M_IX86) && !defined(__i386__) && !defined(VNES_KEEP_MMX_FILTERS)
#  ifndef VNES_X64_NO_MMX
#    define VNES_X64_NO_MMX 1
#  endif
#endif

#ifndef VNES_X64_NO_MMX

// Filtering Render
#include "nx_2xSaI.h"
#include "nx_Super2xSaI.h"
#include "nx_SuperEagle.h"
#include "nx_Scale2x.h"
#include "nx_hq2x.h"	// include lq2x

void	CDirectDraw::nx_2xSaI_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	LPBYTE	pScn = new BYTE[SCREEN_WIDTH*(SCREEN_HEIGHT+6)*sizeof(WORD)];

	// Pre-Rendering
	Render16bpp( lpRdr, &pScn[SCREEN_WIDTH*2*sizeof(WORD)] );

	euI8*	srcPtr   = (euI8*)&pScn[SCREEN_WIDTH*2*sizeof(WORD)];
	euI32	srcPitch = SCREEN_WIDTH*sizeof(WORD);
	euI8*	deltaPtr = (euI8*)lpDlt;
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = (euI32)ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	if( ddsd.ddpfPixelFormat.dwGBitMask == 0x01E0 ) {
	// 555
		cMask = 0x7BDE7BDE7BDE7BDE;
		qMask = 0x739C739C739C739C;
		lMask = 0x0C630C630C630C63;
	} else {
	// 565
		cMask = 0xF7DEF7DEF7DEF7DE;
		qMask = 0xE79CE79CE79CE79C;
		lMask = 0x1863186318631863;
	}

	for( ; height; height-- ) {
		nx_2xSaILine_16bpp_mmx( srcPtr, deltaPtr, srcPitch, width, dstPtr, dstPitch, bForceWrite );
		srcPtr += srcPitch;
		dstPtr += dstPitch * 2;
		deltaPtr += srcPitch;
	}

	delete[] pScn;
}

void	CDirectDraw::nx_2xSaI_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	LPBYTE	pScn = new BYTE[SCREEN_WIDTH*(SCREEN_HEIGHT+6)*sizeof(WORD)];

	// Pre-Rendering
	Render16bppPrefilter( lpRdr, &pScn[SCREEN_WIDTH*2*sizeof(WORD)] );

	euI8*	srcPtr   = (euI8*)&pScn[SCREEN_WIDTH*2*sizeof(WORD)];
	euI32	srcPitch = SCREEN_WIDTH*sizeof(WORD);
	euI8*	deltaPtr = (euI8*)lpDlt;
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = (euI32)ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	// 555
	cMask = 0x7BDE7BDE7BDE7BDE;
	qMask = 0x739C739C739C739C;
	lMask = 0x0C630C630C630C63;

	for( ; height; height-- ) {
		nx_2xSaILine_32bpp_mmx( srcPtr, deltaPtr, srcPitch, width, dstPtr, dstPitch, bForceWrite );
		srcPtr += srcPitch;
		dstPtr += dstPitch * 2;
		deltaPtr += srcPitch;
	}

	delete[] pScn;
}

void	CDirectDraw::nx_Super2xSaI_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	LPBYTE	pScn = new BYTE[SCREEN_WIDTH*(SCREEN_HEIGHT+6)*sizeof(WORD)];

	// Pre-Rendering
	Render16bpp( lpRdr, &pScn[SCREEN_WIDTH*2*sizeof(WORD)] );

	euI8*	srcPtr   = (euI8*)&pScn[SCREEN_WIDTH*2*sizeof(WORD)];
	euI32	srcPitch = SCREEN_WIDTH*sizeof(WORD);
	euI8*	deltaPtr = (euI8*)lpDlt;
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = (euI32)ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	if( ddsd.ddpfPixelFormat.dwGBitMask == 0x01E0 ) {
	// 555
		cMask = 0x7BDE7BDE7BDE7BDE;
		qMask = 0x739C739C739C739C;
		lMask = 0x0C630C630C630C63;
	} else {
	// 565
		cMask = 0xF7DEF7DEF7DEF7DE;
		qMask = 0xE79CE79CE79CE79C;
		lMask = 0x1863186318631863;
	}

	for( ; height; height-- ) {
		nx_Super2xSaILine_16bpp_mmx( srcPtr, deltaPtr, srcPitch, width, dstPtr, dstPitch, bForceWrite );
		srcPtr += srcPitch;
		dstPtr += dstPitch * 2;
		deltaPtr += srcPitch;
	}

	delete[] pScn;
}

void	CDirectDraw::nx_Super2xSaI_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	LPBYTE	pScn = new BYTE[SCREEN_WIDTH*(SCREEN_HEIGHT+6)*sizeof(WORD)];

	// Pre-Rendering
	Render16bppPrefilter( lpRdr, &pScn[SCREEN_WIDTH*2*sizeof(WORD)] );

	euI8*	srcPtr   = (euI8*)&pScn[SCREEN_WIDTH*2*sizeof(WORD)];
	euI32	srcPitch = SCREEN_WIDTH*sizeof(WORD);
	euI8*	deltaPtr = (euI8*)lpDlt;
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = (euI32)ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	// 555
	cMask = 0x7BDE7BDE7BDE7BDE;
	qMask = 0x739C739C739C739C;
	lMask = 0x0C630C630C630C63;

	for( ; height; height-- ) {
		nx_Super2xSaILine_32bpp_mmx( srcPtr, deltaPtr, srcPitch, width, dstPtr, dstPitch, bForceWrite );
		srcPtr += srcPitch;
		dstPtr += dstPitch * 2;
		deltaPtr += srcPitch;
	}

	delete[] pScn;
}

void	CDirectDraw::nx_SuperEagle_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	LPBYTE	pScn = new BYTE[SCREEN_WIDTH*(SCREEN_HEIGHT+6)*sizeof(WORD)];

	// Pre-Rendering
	Render16bpp( lpRdr, &pScn[SCREEN_WIDTH*2*sizeof(WORD)] );

	euI8*	srcPtr   = (euI8*)&pScn[SCREEN_WIDTH*2*sizeof(WORD)];
	euI32	srcPitch = SCREEN_WIDTH*sizeof(WORD);
	euI8*	deltaPtr = (euI8*)lpDlt;
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = (euI32)ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	if( ddsd.ddpfPixelFormat.dwGBitMask == 0x01E0 ) {
	// 555
		cMask = 0x7BDE7BDE7BDE7BDE;
		qMask = 0x739C739C739C739C;
		lMask = 0x0C630C630C630C63;
	} else {
	// 565
		cMask = 0xF7DEF7DEF7DEF7DE;
		qMask = 0xE79CE79CE79CE79C;
		lMask = 0x1863186318631863;
	}

	for( ; height; height-- ) {
		nx_SuperEagleLine_16bpp_mmx( srcPtr, deltaPtr, srcPitch, width, dstPtr, dstPitch, bForceWrite );
		srcPtr += srcPitch;
		dstPtr += dstPitch * 2;
		deltaPtr += srcPitch;
	}

	delete[] pScn;
}

void	CDirectDraw::nx_SuperEagle_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	LPBYTE	pScn = new BYTE[SCREEN_WIDTH*(SCREEN_HEIGHT+6)*sizeof(WORD)];

	// Pre-Rendering
	Render16bppPrefilter( lpRdr, &pScn[SCREEN_WIDTH*2*sizeof(WORD)] );

	euI8*	srcPtr   = (euI8*)&pScn[SCREEN_WIDTH*2*sizeof(WORD)];
	euI32	srcPitch = SCREEN_WIDTH*sizeof(WORD);
	euI8*	deltaPtr = (euI8*)lpDlt;
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = (euI32)ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	// 555
	cMask = 0x7BDE7BDE7BDE7BDE;
	qMask = 0x739C739C739C739C;
	lMask = 0x0C630C630C630C63;

	for( ; height; height-- ) {
		nx_SuperEagleLine_32bpp_mmx( srcPtr, deltaPtr, srcPitch, width, dstPtr, dstPitch, bForceWrite );
		srcPtr += srcPitch;
		dstPtr += dstPitch * 2;
		deltaPtr += srcPitch;
	}

	delete[] pScn;
}

void	CDirectDraw::nx_Scale2x_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	// Pre-Rendering
	Render16bpp( lpRdr, lpDlt );

	euI8*	srcPtr   = (euI8*)lpDlt;
	euI32	srcPitch = SCREEN_WIDTH * sizeof(euI16);
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	euI16	*dst0 = (euI16*)dstPtr;
	euI16	*dst1 = dst0 + (dstPitch/2);
	euI16	*src0 = (euI16*)lpDlt;
	euI16	*src1 = src0 + (srcPitch/2);
	euI16	*src2 = src1 + (srcPitch/2);

	internal_scale2x_16_mmx( dst0, dst1, src0, src0, src1, width );

	int count = height;
	count -= 2;
	while( count ) {
		dst0 += dstPitch;
		dst1 += dstPitch;
		internal_scale2x_16_mmx( dst0, dst1, src0, src1, src2, width );
		src0 = src1;
		src1 = src2;
		src2 += srcPitch/2;
		--count;
	}
	dst0 += dstPitch;
	dst1 += dstPitch;
	internal_scale2x_16_mmx( dst0, dst1, src0, src1, src1, width );
}

void	CDirectDraw::nx_Scale2x_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	// Pre-Rendering
	Render32bpp( lpRdr, lpDlt );

	euI8*	srcPtr   = (euI8*)lpDlt;
	euI32	srcPitch = SCREEN_WIDTH * sizeof(euI32);
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	euI32	*dst0 = (euI32*)dstPtr;
	euI32	*dst1 = dst0 + (dstPitch/4);
	euI32	*src0 = (euI32*)lpDlt;
	euI32	*src1 = src0 + (srcPitch/4);
	euI32	*src2 = src1 + (srcPitch/4);

	internal_scale2x_32_mmx( dst0, dst1, src0, src0, src1, width );

	int count = height;
	count -= 2;
	while( count ) {
		dst0 += dstPitch/2;
		dst1 += dstPitch/2;
		internal_scale2x_32_mmx( dst0, dst1, src0, src1, src2, width );
		src0 = src1;
		src1 = src2;
		src2 += srcPitch/4;
		--count;
	}
	dst0 += dstPitch/2;
	dst1 += dstPitch/2;
	internal_scale2x_32_mmx( dst0, dst1, src0, src1, src1, width );
}

void	CDirectDraw::nx_hq2x_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	// Pre-Rendering
	Render16bpp( lpRdr, lpDlt );

	euI8*	srcPtr   = (euI8*)lpDlt;
	euI32	srcPitch = SCREEN_WIDTH * sizeof(euI16);
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	euI16	*dst0 = (euI16*)dstPtr;
	euI16	*dst1 = dst0 + (dstPitch/2);
	euI16	*src0 = (euI16*)lpDlt;
	euI16	*src1 = src0 + (srcPitch/2);
	euI16	*src2 = src1 + (srcPitch/2);

	if( ddsd.ddpfPixelFormat.dwGBitMask == 0x01E0 ) {
		interp_set( 15 );
	} else {
		interp_set( 16 );
	}

	hq2x_16_def( dst0, dst1, src0, src0, src1, width );

	int count = height;
	count -= 2;
	while( count ) {
		dst0 += dstPitch;
		dst1 += dstPitch;
		hq2x_16_def( dst0, dst1, src0, src1, src2, width );
		src0 = src1;
		src1 = src2;
		src2 += srcPitch/2;
		--count;
	}
	dst0 += dstPitch;
	dst1 += dstPitch;
	hq2x_16_def( dst0, dst1, src0, src1, src1, width );
}

void	CDirectDraw::nx_hq2x_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	// Pre-Rendering
	Render32bpp( lpRdr, lpDlt );

	euI8*	srcPtr   = (euI8*)lpDlt;
	euI32	srcPitch = SCREEN_WIDTH * sizeof(euI32);
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	euI32	*dst0 = (euI32*)dstPtr;
	euI32	*dst1 = dst0 + (dstPitch/4);
	euI32	*src0 = (euI32*)lpDlt;
	euI32	*src1 = src0 + (srcPitch/4);
	euI32	*src2 = src1 + (srcPitch/4);

	interp_set( 32 );

	hq2x_32_def( dst0, dst1, src0, src0, src1, width );

	int count = height;
	count -= 2;
	while( count ) {
		dst0 += dstPitch/2;
		dst1 += dstPitch/2;
		hq2x_32_def( dst0, dst1, src0, src1, src2, width );
		src0 = src1;
		src1 = src2;
		src2 += srcPitch/4;
		--count;
	}
	dst0 += dstPitch/2;
	dst1 += dstPitch/2;
	hq2x_32_def( dst0, dst1, src0, src1, src1, width );
}

void	CDirectDraw::nx_lq2x_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	// Pre-Rendering
	Render16bpp( lpRdr, lpDlt );

	euI8*	srcPtr   = (euI8*)lpDlt;
	euI32	srcPitch = SCREEN_WIDTH * sizeof(euI16);
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	euI16	*dst0 = (euI16*)dstPtr;
	euI16	*dst1 = dst0 + (dstPitch/2);
	euI16	*src0 = (euI16*)lpDlt;
	euI16	*src1 = src0 + (srcPitch/2);
	euI16	*src2 = src1 + (srcPitch/2);

	if( ddsd.ddpfPixelFormat.dwGBitMask == 0x01E0 ) {
		interp_set( 15 );
	} else {
		interp_set( 16 );
	}

	lq2x_16_def( dst0, dst1, src0, src0, src1, width );

	int count = height;
	count -= 2;
	while( count ) {
		dst0 += dstPitch;
		dst1 += dstPitch;
		lq2x_16_def( dst0, dst1, src0, src1, src2, width );
		src0 = src1;
		src1 = src2;
		src2 += srcPitch/2;
		--count;
	}
	dst0 += dstPitch;
	dst1 += dstPitch;
	lq2x_16_def( dst0, dst1, src0, src1, src1, width );
}

void	CDirectDraw::nx_lq2x_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	// Pre-Rendering
	Render32bpp( lpRdr, lpDlt );

	euI8*	srcPtr   = (euI8*)lpDlt;
	euI32	srcPitch = SCREEN_WIDTH * sizeof(euI32);
	euI8*	dstPtr   = (euI8*)ddsd.lpSurface;
	euI32	dstPitch = ddsd.lPitch;
	int	width    = SCREEN_WIDTH;
	int	height   = SCREEN_HEIGHT;

	euI32	*dst0 = (euI32*)dstPtr;
	euI32	*dst1 = dst0 + (dstPitch/4);
	euI32	*src0 = (euI32*)lpDlt;
	euI32	*src1 = src0 + (srcPitch/4);
	euI32	*src2 = src1 + (srcPitch/4);

	interp_set( 32 );

	lq2x_32_def( dst0, dst1, src0, src0, src1, width );

	int count = height;
	count -= 2;
	while( count ) {
		dst0 += dstPitch/2;
		dst1 += dstPitch/2;
		lq2x_32_def( dst0, dst1, src0, src1, src2, width );
		src0 = src1;
		src1 = src2;
		src2 += srcPitch/4;
		--count;
	}
	dst0 += dstPitch/2;
	dst1 += dstPitch/2;
	lq2x_32_def( dst0, dst1, src0, src1, src1, width );
}

#else	// VNES_X64_NO_MMX

//------------------------------------------------------------------
// x64 降级实现：滤镜暂时走 Double 放大�?x，无扫描线）�?
// 画面尺寸与滤镜模式一致，因此上层逻辑、delta 缓冲布局都不用改�?
//------------------------------------------------------------------
void	CDirectDraw::nx_2xSaI_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render16bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_2xSaI_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render32bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_Super2xSaI_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render16bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_Super2xSaI_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render32bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_SuperEagle_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render16bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_SuperEagle_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render32bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_Scale2x_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render16bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_Scale2x_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render32bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_hq2x_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render16bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_hq2x_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render32bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_lq2x_16bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render16bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

void	CDirectDraw::nx_lq2x_32bpp( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{	Render32bpp_Double( lpRdr, lpDlt, ddsd, bForceWrite );	}

#endif	// VNES_X64_NO_MMX
