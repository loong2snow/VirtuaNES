//
// 16bit Normal
//
// 汇编语义：一次读 4 字节 = 4 个索引，�?DWORD 调色板取�?16 位写出（mov [edi], cx）；
// delta 每行 4 字节行头�?colormode，之后每 4 字节一组与上一帧比较，相同则整组跳过�?
//
void	CDirectDraw::Render16bpp_Normal( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;
	const int pitch = (int)ddsd.lPitch;

	for( int i = 0; i < H; i++ ) {
		const int cm = m_LineColormode[i];
		const vnesx::u32* pal = ( cm & 0x80 )
			? (const vnesx::u32*)m_mnPalette[cm & 0x07]
			: (const vnesx::u32*)m_cnPalette[cm & 0x07];

		vnesx::u8* rowDlt = (vnesx::u8*)lpDlt + i*(W+4);
		const bool fw = ( bForceWrite != 0 ) || ( rowDlt[0] != (vnesx::u8)cm );
		if( fw ) rowDlt[0] = (vnesx::u8)cm;

		vnesx::Row<vnesx::Put16x1>( (const vnesx::u8*)lpRdr + i*RENDER_WIDTH,
		                            (vnesx::u8*)ddsd.lpSurface + i*pitch,
		                            pal, W, rowDlt+4, fw, true );
	}
}

//
// 16bit Scanline
//
// 两遍写法：第一遍用正常调色板，第二遍用暗色调色板（cs/ms）�?
// delta 只在最后一遍回写（原注释�?回目は書き込む」）�?
//
void	CDirectDraw::Render16bpp_Scanline( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;
	const int pitch = (int)ddsd.lPitch;

	for( int i = 0; i < H; i++ ) {
		const int cm = m_LineColormode[i];
		const vnesx::u32* pal, *palScan;
		if( !(cm & 0x80) ) {
			pal     = (const vnesx::u32*)m_cnPalette[cm & 0x07];
			palScan = (const vnesx::u32*)m_csPalette[cm & 0x07];
		} else {
			pal     = (const vnesx::u32*)m_mnPalette[cm & 0x07];
			palScan = (const vnesx::u32*)m_msPalette[cm & 0x07];
		}

		vnesx::u8* rowDlt = (vnesx::u8*)lpDlt + i*(W+4);
		const bool fw = ( bForceWrite != 0 ) || ( rowDlt[0] != (vnesx::u8)cm );
		if( fw ) rowDlt[0] = (vnesx::u8)cm;

		const vnesx::u8* s = (const vnesx::u8*)lpRdr + i*RENDER_WIDTH;
		vnesx::u8*       d = (vnesx::u8*)ddsd.lpSurface + i*2*pitch;

		vnesx::Row<vnesx::Put16x1>( s, d,         pal,     W, rowDlt+4, fw, false );
		vnesx::Row<vnesx::Put16x1>( s, d + pitch, palScan, W, rowDlt+4, fw, true  );
	}
}

//
// 16bit Double
//
void	CDirectDraw::Render16bpp_Double( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;
	const int pitch = (int)ddsd.lPitch;

	for( int i = 0; i < H; i++ ) {
		const int cm = m_LineColormode[i];
		const vnesx::u32* pal = ( cm & 0x80 )
			? (const vnesx::u32*)m_mnPalette[cm & 0x07]
			: (const vnesx::u32*)m_cnPalette[cm & 0x07];

		vnesx::u8* rowDlt = (vnesx::u8*)lpDlt + i*(W+4);
		const bool fw = ( bForceWrite != 0 ) || ( rowDlt[0] != (vnesx::u8)cm );
		if( fw ) rowDlt[0] = (vnesx::u8)cm;

		const vnesx::u8* s = (const vnesx::u8*)lpRdr + i*RENDER_WIDTH;
		vnesx::u8*       d = (vnesx::u8*)ddsd.lpSurface + i*2*pitch;

		vnesx::Row<vnesx::Put16x2>( s, d,         pal, W, rowDlt+4, fw, false );
		vnesx::Row<vnesx::Put16x2>( s, d + pitch, pal, W, rowDlt+4, fw, true  );
	}
}

//
// 16bit Double Scanline
//
void	CDirectDraw::Render16bpp_DoubleScanline( LPBYTE lpRdr, LPBYTE lpDlt, DDSURFACEDESC2& ddsd, BOOL bForceWrite )
{
	const int W = SCREEN_WIDTH;
	const int H = SCREEN_HEIGHT;
	const int pitch = (int)ddsd.lPitch;

	for( int i = 0; i < H; i++ ) {
		const int cm = m_LineColormode[i];
		const vnesx::u32* pal, *palScan;
		if( !(cm & 0x80) ) {
			pal     = (const vnesx::u32*)m_cnPalette[cm & 0x07];
			palScan = (const vnesx::u32*)m_csPalette[cm & 0x07];
		} else {
			pal     = (const vnesx::u32*)m_mnPalette[cm & 0x07];
			palScan = (const vnesx::u32*)m_msPalette[cm & 0x07];
		}

		vnesx::u8* rowDlt = (vnesx::u8*)lpDlt + i*(W+4);
		const bool fw = ( bForceWrite != 0 ) || ( rowDlt[0] != (vnesx::u8)cm );
		if( fw ) rowDlt[0] = (vnesx::u8)cm;

		const vnesx::u8* s = (const vnesx::u8*)lpRdr + i*RENDER_WIDTH;
		vnesx::u8*       d = (vnesx::u8*)ddsd.lpSurface + i*2*pitch;

		vnesx::Row<vnesx::Put16x2>( s, d,         pal,     W, rowDlt+4, fw, false );
		vnesx::Row<vnesx::Put16x2>( s, d + pitch, palScan, W, rowDlt+4, fw, true  );
	}
}
