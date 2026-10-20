//
// 8bit Normal
//
// 原实现：or eax, 0x40404040 —�?8bpp 不查调色板表，索引直接写出，
// 靠高 2 位（0x40 / 0x80）切到逻辑调色板的不同段落实现扫描线�?
// MMX 版用 punpcklbw 做水平复�?+ por 0x4040404040404040 / 0x8080...
//�?bpp 模式下不使用 lpDlt / delta，这两个参数原实现也没用到）
//
void	CDirectDraw::Render8bpp_Normal( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;
	const int pitch = (int)ddsd.lPitch;

	for( int i = 0; i < H; i++ )
		vnesx::Row8( (const vnesx::u8*)lpRdr + i*RENDER_WIDTH,
		             (vnesx::u8*)ddsd.lpSurface + i*pitch,
		             W, 0x40, 1 );
}

//
// 8bit Scanline
//
void	CDirectDraw::Render8bpp_Scanline( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;
	const int pitch = (int)ddsd.lPitch;

	for( int i = 0; i < H; i++ ) {
		const vnesx::u8* s = (const vnesx::u8*)lpRdr + i*RENDER_WIDTH;
		vnesx::u8*       d = (vnesx::u8*)ddsd.lpSurface + i*2*pitch;

		vnesx::Row8( s, d,         W, 0x40, 1 );		// 亮行
		vnesx::Row8( s, d + pitch, W, 0x80, 1 );		// 暗行
	}
}

//
// 8bit Double
//
void	CDirectDraw::Render8bpp_Double( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;
	const int pitch = (int)ddsd.lPitch;

	for( int i = 0; i < H; i++ ) {
		const vnesx::u8* s = (const vnesx::u8*)lpRdr + i*RENDER_WIDTH;
		vnesx::u8*       d = (vnesx::u8*)ddsd.lpSurface + i*2*pitch;

		vnesx::Row8( s, d,         W, 0x40, 2 );
		vnesx::Row8( s, d + pitch, W, 0x40, 2 );
	}
}

//
// 8bit Double Scanline
//
void	CDirectDraw::Render8bpp_DoubleScanline( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;
	const int pitch = (int)ddsd.lPitch;

	for( int i = 0; i < H; i++ ) {
		const vnesx::u8* s = (const vnesx::u8*)lpRdr + i*RENDER_WIDTH;
		vnesx::u8*       d = (vnesx::u8*)ddsd.lpSurface + i*2*pitch;

		vnesx::Row8( s, d,         W, 0x40, 2 );		// 亮行
		vnesx::Row8( s, d + pitch, W, 0x80, 2 );		// 暗行
	}
}
