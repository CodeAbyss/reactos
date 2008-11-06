/*
 *  ReactOS W32 Subsystem
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003 ReactOS Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <w32k.h>

#define NDEBUG
#include <debug.h>

/*
 * a couple macros to fill a single pixel or a line
 */
#define PUTPIXEL(x,y,BrushInst)        \
  ret = ret && IntEngLineTo(&BitmapObj->SurfObj, \
       dc->CombinedClip,                         \
       &BrushInst.BrushObject,                   \
       x, y, (x)+1, y,                           \
       &RectBounds,                              \
       ROP2_TO_MIX(Dc_Attr->jROP2));

#define PUTLINE(x1,y1,x2,y2,BrushInst) \
  ret = ret && IntEngLineTo(&BitmapObj->SurfObj, \
       dc->CombinedClip,                         \
       &BrushInst.BrushObject,                   \
       x1, y1, x2, y2,                           \
       &RectBounds,                              \
       ROP2_TO_MIX(Dc_Attr->jROP2));

#define Rsin(d) ((d) == 0.0 ? 0.0 : ((d) == 90.0 ? 1.0 : sin(d*M_PI/180.0)))
#define Rcos(d) ((d) == 0.0 ? 1.0 : ((d) == 90.0 ? 0.0 : cos(d*M_PI/180.0)))

BOOL FASTCALL IntFillEllipse( PDC dc, INT XLeft, INT YLeft, INT Width, INT Height);
BOOL FASTCALL IntDrawEllipse( PDC dc, INT XLeft, INT YLeft, INT Width, INT Height, PGDIBRUSHOBJ PenBrushObj);
BOOL FASTCALL IntFillRoundRect( PDC dc, INT Left, INT Top, INT Right, INT Bottom, INT Wellipse, INT Hellipse);
BOOL FASTCALL IntDrawRoundRect( PDC dc, INT Left, INT Top, INT Right, INT Bottom, INT Wellipse, INT Hellipse, PGDIBRUSHOBJ PenBrushObj);

BOOL FASTCALL
IntGdiPolygon(PDC    dc,
              PPOINT Points,
              int    Count)
{
    BITMAPOBJ *BitmapObj;
    PGDIBRUSHOBJ PenBrushObj, FillBrushObj;
    GDIBRUSHINST PenBrushInst, FillBrushInst;
    BOOL ret = FALSE; // default to failure
    RECTL DestRect;
    int CurrentPoint;
    PDC_ATTR Dc_Attr;

    ASSERT(dc); // caller's responsibility to pass a valid dc

    if (!Points || Count < 2 )
    {
        SetLastWin32Error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    Dc_Attr = dc->pDc_Attr;
    if (!Dc_Attr) Dc_Attr = &dc->Dc_Attr;

    /* Convert to screen coordinates */
    IntLPtoDP(dc, Points, Count);
    for (CurrentPoint = 0; CurrentPoint < Count; CurrentPoint++)
    {
        Points[CurrentPoint].x += dc->ptlDCOrig.x;
        Points[CurrentPoint].y += dc->ptlDCOrig.y;
    }
    // No need to have path here.
    {
        DestRect.left   = Points[0].x;
        DestRect.right  = Points[0].x;
        DestRect.top    = Points[0].y;
        DestRect.bottom = Points[0].y;

        for (CurrentPoint = 1; CurrentPoint < Count; ++CurrentPoint)
        {
            DestRect.left     = min(DestRect.left, Points[CurrentPoint].x);
            DestRect.right    = max(DestRect.right, Points[CurrentPoint].x);
            DestRect.top      = min(DestRect.top, Points[CurrentPoint].y);
            DestRect.bottom   = max(DestRect.bottom, Points[CurrentPoint].y);
        }

        /* Special locking order to avoid lock-ups */
        FillBrushObj = BRUSHOBJ_LockBrush(Dc_Attr->hbrush);
        PenBrushObj = PENOBJ_LockPen(Dc_Attr->hpen);
        BitmapObj = BITMAPOBJ_LockBitmap(dc->w.hBitmap);
        /* FIXME - BitmapObj can be NULL!!!! don't assert but handle this case gracefully! */
        ASSERT(BitmapObj);

        /* Now fill the polygon with the current brush. */
        if (FillBrushObj && !(FillBrushObj->flAttrs & GDIBRUSH_IS_NULL))
        {
            IntGdiInitBrushInstance(&FillBrushInst, FillBrushObj, dc->XlateBrush);
            ret = FillPolygon ( dc, BitmapObj, &FillBrushInst.BrushObject, ROP2_TO_MIX(Dc_Attr->jROP2), Points, Count, DestRect );
        }
        if (FillBrushObj)
            BRUSHOBJ_UnlockBrush(FillBrushObj);

        // Draw the Polygon Edges with the current pen ( if not a NULL pen )
        if (PenBrushObj && !(PenBrushObj->flAttrs & GDIBRUSH_IS_NULL))
        {
            int i;

            IntGdiInitBrushInstance(&PenBrushInst, PenBrushObj, dc->XlatePen);

            for (i = 0; i < Count-1; i++)
            {

// DPRINT1("Polygon Making line from (%d,%d) to (%d,%d)\n",
//                                 Points[0].x, Points[0].y,
//                                 Points[1].x, Points[1].y );

                ret = IntEngLineTo(&BitmapObj->SurfObj,
                                   dc->CombinedClip,
                                   &PenBrushInst.BrushObject,
                                   Points[i].x,          /* From */
                                   Points[i].y,
                                   Points[i+1].x,          /* To */
                                   Points[i+1].y,
                                   &DestRect,
                                   ROP2_TO_MIX(Dc_Attr->jROP2)); /* MIX */
                if (!ret) break;
            }
            /* Close the polygon */
            if (ret)
            {
                ret = IntEngLineTo(&BitmapObj->SurfObj,
                                   dc->CombinedClip,
                                   &PenBrushInst.BrushObject,
                                   Points[Count-1].x, /* From */
                                   Points[Count-1].y,
                                   Points[0].x,       /* To */
                                   Points[0].y,
                                   &DestRect,
                                   ROP2_TO_MIX(Dc_Attr->jROP2)); /* MIX */
            }
        }
        if (PenBrushObj)
            PENOBJ_UnlockPen(PenBrushObj);
    }
    BITMAPOBJ_UnlockBitmap(BitmapObj);

    return ret;
}

BOOL FASTCALL
IntGdiPolyPolygon(DC      *dc,
                  LPPOINT Points,
                  PULONG  PolyCounts,
                  int     Count)
{
    if (PATH_IsPathOpen(dc->DcLevel))
        return PATH_PolyPolygon ( dc, Points, (PINT)PolyCounts, Count);

    while (--Count >=0)
    {
        if (!IntGdiPolygon ( dc, Points, *PolyCounts ))
            return FALSE;
        Points+=*PolyCounts++;
    }
    return TRUE;
}



/******************************************************************************/

/*
 * NtGdiEllipse
 *
 * Author
 *    Filip Navara
 *
 * Remarks
 *    This function uses optimized Bresenham's ellipse algorithm. It draws
 *    four lines of the ellipse in one pass.
 *
 */

BOOL STDCALL
NtGdiEllipse(
    HDC hDC,
    int Left,
    int Top,
    int Right,
    int Bottom)
{
    PDC dc;
    PDC_ATTR Dc_Attr;
    RECTL RectBounds;
    PGDIBRUSHOBJ PenBrushObj;
    BOOL ret = TRUE;
    LONG PenWidth, PenOrigWidth;
    LONG RadiusX, RadiusY, CenterX, CenterY;

    if ((Left == Right) || (Top == Bottom)) return TRUE;

    dc = DC_LockDc(hDC);
    if (dc == NULL)
    {
       SetLastWin32Error(ERROR_INVALID_HANDLE);
       return FALSE;
    }
    if (dc->DC_Type == DC_TYPE_INFO)
    {
       DC_UnlockDc(dc);
       /* Yes, Windows really returns TRUE in this case */
       return TRUE;
    }

    if (PATH_IsPathOpen(dc->DcLevel))
    {
        ret = PATH_Ellipse(dc, Left, Top, Right, Bottom);
        DC_UnlockDc(dc);
        return ret;
    }

    if (Right < Left)
    {
       INT tmp = Right; Right = Left; Left = tmp;
    }
    if (Bottom < Top)
    {
       INT tmp = Bottom; Bottom = Top; Top = tmp;
    }

    Dc_Attr = dc->pDc_Attr;
    if(!Dc_Attr) Dc_Attr = &dc->Dc_Attr;

    PenBrushObj = PENOBJ_LockPen(Dc_Attr->hpen);
    if (NULL == PenBrushObj)
    {
        DPRINT1("Ellipse Fail 1\n");
        DC_UnlockDc(dc);
        SetLastWin32Error(ERROR_INTERNAL_ERROR);
        return FALSE;
    }

    PenOrigWidth = PenWidth = PenBrushObj->ptPenWidth.x;
    if (PenBrushObj->ulPenStyle == PS_NULL) PenWidth = 0;

    if (PenBrushObj->ulPenStyle == PS_INSIDEFRAME)
    {
       if (2*PenWidth > (Right - Left)) PenWidth = (Right -Left + 1)/2;
       if (2*PenWidth > (Bottom - Top)) PenWidth = (Bottom -Top + 1)/2;
       Left   += PenWidth / 2;
       Right  -= (PenWidth - 1) / 2;
       Top    += PenWidth / 2;
       Bottom -= (PenWidth - 1) / 2;
    }

    if (!PenWidth) PenWidth = 1;
    PenBrushObj->ptPenWidth.x = PenWidth;  

    RectBounds.left   = Left;
    RectBounds.right  = Right;
    RectBounds.top    = Top;
    RectBounds.bottom = Bottom;
                
    IntLPtoDP(dc, (LPPOINT)&RectBounds, 2);
 
    RectBounds.left += dc->ptlDCOrig.x;
    RectBounds.right += dc->ptlDCOrig.x;
    RectBounds.top += dc->ptlDCOrig.y;
    RectBounds.bottom += dc->ptlDCOrig.y;

    // Setup for dynamic width and height.
    RadiusX = max((RectBounds.right - RectBounds.left) / 2, 2); // Needs room
    RadiusY = max((RectBounds.bottom - RectBounds.top) / 2, 2);
    CenterX = (RectBounds.right + RectBounds.left) / 2;
    CenterY = (RectBounds.bottom + RectBounds.top) / 2;

    DPRINT("Ellipse 1: Left: %d, Top: %d, Right: %d, Bottom: %d\n",
               RectBounds.left,RectBounds.top,RectBounds.right,RectBounds.bottom);

    DPRINT("Ellipse 2: XLeft: %d, YLeft: %d, Width: %d, Height: %d\n",
               CenterX - RadiusX, CenterY + RadiusY, RadiusX*2, RadiusY*2);

    ret = IntFillEllipse( dc,
                          CenterX - RadiusX,
                          CenterY - RadiusY,
                          RadiusX*2, // Width
                          RadiusY*2); // Height
    if (ret)
       ret = IntDrawEllipse( dc,
                             CenterX - RadiusX,
                             CenterY - RadiusY,
                             RadiusX*2, // Width
                             RadiusY*2, // Height
                             PenBrushObj);

    PenBrushObj->ptPenWidth.x = PenOrigWidth;
    PENOBJ_UnlockPen(PenBrushObj);
    DC_UnlockDc(dc);
    DPRINT("Ellipse Exit.\n");
    return ret;
}

#if 0

//When the fill mode is ALTERNATE, GDI fills the area between odd-numbered and
//even-numbered polygon sides on each scan line. That is, GDI fills the area between the
//first and second side, between the third and fourth side, and so on.

//WINDING Selects winding mode (fills any region with a nonzero winding value).
//When the fill mode is WINDING, GDI fills any region that has a nonzero winding value.
//This value is defined as the number of times a pen used to draw the polygon would go around the region.
//The direction of each edge of the polygon is important.

extern BOOL FillPolygon(PDC dc,
                            SURFOBJ *SurfObj,
                            PBRUSHOBJ BrushObj,
                            MIX RopMode,
                            CONST PPOINT Points,
                            int Count,
                            RECTL BoundRect);

#endif


ULONG_PTR
STDCALL
NtGdiPolyPolyDraw( IN HDC hDC,
                   IN PPOINT UnsafePoints,
                   IN PULONG UnsafeCounts,
                   IN ULONG Count,
                   IN INT iFunc )
{
    DC *dc;
    PVOID pTemp;
    LPPOINT SafePoints;
    PULONG SafeCounts;
    NTSTATUS Status = STATUS_SUCCESS;
    BOOL Ret = TRUE;
    INT nPoints = 0, nMaxPoints = 0, nInvalid = 0, i;

    if (!UnsafePoints || !UnsafeCounts ||
        Count == 0 || iFunc == 0 || iFunc > GdiPolyPolyRgn)
    {
        /* Windows doesn't set last error */
        return FALSE;
    }

    _SEH_TRY
    {
        ProbeForRead(UnsafePoints, Count * sizeof(POINT), 1);
        ProbeForRead(UnsafeCounts, Count * sizeof(ULONG), 1);

        /* Count points and validate poligons */
        for (i = 0; i < Count; i++)
        {
            if (UnsafeCounts[i] < 2)
            {
                nInvalid++;
            }
            nPoints += UnsafeCounts[i];
            nMaxPoints = max(nMaxPoints, UnsafeCounts[i]);
        }
    }
    _SEH_HANDLE
    {
        Status = _SEH_GetExceptionCode();
    }
    _SEH_END;

    if (!NT_SUCCESS(Status))
    {
        /* Windows doesn't set last error */
        return FALSE;
    }

    if (nPoints == 0 || nPoints < nMaxPoints)
    {
        /* If all polygon counts are zero, or we have overflow,
           return without setting a last error code. */
        return FALSE;
    }

    if (nInvalid != 0)
    {
        /* If at least one poly count is 0 or 1, fail */
        SetLastWin32Error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* Allocate one buffer for both counts and points */
    pTemp = ExAllocatePoolWithTag(PagedPool,
                                  Count * sizeof(ULONG) + nPoints * sizeof(POINT),
                                  TAG_SHAPE);
    if (!pTemp)
    {
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    SafeCounts = pTemp;
    SafePoints = (PVOID)(SafeCounts + Count);

    _SEH_TRY
    {
        /* Pointers already probed! */
        RtlCopyMemory(SafeCounts, UnsafeCounts, Count * sizeof(ULONG));
        RtlCopyMemory(SafePoints, UnsafePoints, nPoints * sizeof(POINT));
    }
    _SEH_HANDLE
    {
        Status = _SEH_GetExceptionCode();
    }
    _SEH_END;

    if (!NT_SUCCESS(Status))
    {
        ExFreePoolWithTag(pTemp, TAG_SHAPE);
        return FALSE;
    }

    /* Special handling for GdiPolyPolyRgn */
    if (iFunc == GdiPolyPolyRgn)
    {
        HRGN hRgn;
        hRgn = IntCreatePolyPolygonRgn(SafePoints, SafeCounts, Count, (INT_PTR)hDC);
        ExFreePoolWithTag(pTemp, TAG_SHAPE);
        return (ULONG_PTR)hRgn;
    }

    dc = DC_LockDc(hDC);
    if (!dc)
    {
        SetLastWin32Error(ERROR_INVALID_HANDLE);
        ExFreePool(pTemp);
        return FALSE;
    }

    if (dc->DC_Type == DC_TYPE_INFO)
    {
        DC_UnlockDc(dc);
        ExFreePool(pTemp);
        /* Yes, Windows really returns TRUE in this case */
        return TRUE;
    }

    /* Perform the actual work */
    switch (iFunc)
    {
        case GdiPolyPolygon:
            Ret = IntGdiPolyPolygon(dc, SafePoints, SafeCounts, Count);
            break;
        case GdiPolyPolyLine:
            Ret = IntGdiPolyPolyline(dc, SafePoints, SafeCounts, Count);
            break;
        case GdiPolyBezier:
            Ret = IntGdiPolyBezier(dc, SafePoints, *SafeCounts);
            break;
        case GdiPolyLineTo:
            Ret = IntGdiPolylineTo(dc, SafePoints, *SafeCounts);
            break;
        case GdiPolyBezierTo:
            Ret = IntGdiPolyBezierTo(dc, SafePoints, *SafeCounts);
            break;
        default:
            SetLastWin32Error(ERROR_INVALID_PARAMETER);
            Ret = FALSE;
    }

    /* Cleanup and return */
    DC_UnlockDc(dc);
    ExFreePool(pTemp);

    return (ULONG_PTR)Ret;
}


BOOL
FASTCALL
IntRectangle(PDC dc,
             int LeftRect,
             int TopRect,
             int RightRect,
             int BottomRect)
{
    BITMAPOBJ *BitmapObj = NULL;
    PGDIBRUSHOBJ PenBrushObj = NULL, FillBrushObj = NULL;
    GDIBRUSHINST PenBrushInst, FillBrushInst;
    BOOL       ret = FALSE; // default to failure
    RECTL      DestRect;
    MIX        Mix;
    PDC_ATTR Dc_Attr;

    ASSERT ( dc ); // caller's responsibility to set this up

    Dc_Attr = dc->pDc_Attr;
    if(!Dc_Attr) Dc_Attr = &dc->Dc_Attr;

    /* Do we rotate or shear? */
    if (!(dc->DcLevel.mxWorldToDevice.flAccel & MX_SCALE))
    {

        POINTL DestCoords[4];
        ULONG  PolyCounts = 4;
        DestCoords[0].x = DestCoords[3].x = LeftRect;
        DestCoords[0].y = DestCoords[1].y = TopRect;
        DestCoords[1].x = DestCoords[2].x = RightRect;
        DestCoords[2].y = DestCoords[3].y = BottomRect;
        // Use IntGdiPolyPolygon so to support PATH.
        return IntGdiPolyPolygon(dc, DestCoords, &PolyCounts, 1);
    }
    // Rectangle Path only.
    if ( PATH_IsPathOpen(dc->DcLevel) )
    {
        return PATH_Rectangle ( dc, LeftRect, TopRect, RightRect, BottomRect );
    }

    DestRect.left = LeftRect;
    DestRect.right = RightRect;
    DestRect.top = TopRect;
    DestRect.bottom = BottomRect;

    IntLPtoDP(dc, (LPPOINT)&DestRect, 2);

    DestRect.left   += dc->ptlDCOrig.x;
    DestRect.right  += dc->ptlDCOrig.x;
    DestRect.top    += dc->ptlDCOrig.y;
    DestRect.bottom += dc->ptlDCOrig.y;

    /* In GM_COMPATIBLE, don't include bottom and right edges */
    if (IntGetGraphicsMode(dc) == GM_COMPATIBLE)
    {
        DestRect.right--;
        DestRect.bottom--;
    }

    /* Special locking order to avoid lock-ups! */
    FillBrushObj = BRUSHOBJ_LockBrush(Dc_Attr->hbrush);
    PenBrushObj = PENOBJ_LockPen(Dc_Attr->hpen);
    if (!PenBrushObj)
    {
        ret = FALSE;
        goto cleanup;
    }
    BitmapObj = BITMAPOBJ_LockBitmap(dc->w.hBitmap);
    if (!BitmapObj)
    {
        ret = FALSE;
        goto cleanup;
    }

    if ( FillBrushObj )
    {
        if (!(FillBrushObj->flAttrs & GDIBRUSH_IS_NULL))
        {
            IntGdiInitBrushInstance(&FillBrushInst, FillBrushObj, dc->XlateBrush);
            ret = IntEngBitBlt(&BitmapObj->SurfObj,
                               NULL,
                               NULL,
                               dc->CombinedClip,
                               NULL,
                               &DestRect,
                               NULL,
                               NULL,
                               &FillBrushInst.BrushObject,
                               NULL,
                               ROP3_TO_ROP4(PATCOPY));
        }
    }

    IntGdiInitBrushInstance(&PenBrushInst, PenBrushObj, dc->XlatePen);

    // Draw the rectangle with the current pen

    ret = TRUE; // change default to success

    if (!(PenBrushObj->flAttrs & GDIBRUSH_IS_NULL))
    {
        Mix = ROP2_TO_MIX(Dc_Attr->jROP2);
        ret = ret && IntEngLineTo(&BitmapObj->SurfObj,
                                  dc->CombinedClip,
                                  &PenBrushInst.BrushObject,
                                  DestRect.left, DestRect.top, DestRect.right, DestRect.top,
                                  &DestRect, // Bounding rectangle
                                  Mix);

        ret = ret && IntEngLineTo(&BitmapObj->SurfObj,
                                  dc->CombinedClip,
                                  &PenBrushInst.BrushObject,
                                  DestRect.right, DestRect.top, DestRect.right, DestRect.bottom,
                                  &DestRect, // Bounding rectangle
                                  Mix);

        ret = ret && IntEngLineTo(&BitmapObj->SurfObj,
                                  dc->CombinedClip,
                                  &PenBrushInst.BrushObject,
                                  DestRect.right, DestRect.bottom, DestRect.left, DestRect.bottom,
                                  &DestRect, // Bounding rectangle
                                  Mix);

        ret = ret && IntEngLineTo(&BitmapObj->SurfObj,
                                  dc->CombinedClip,
                                  &PenBrushInst.BrushObject,
                                  DestRect.left, DestRect.bottom, DestRect.left, DestRect.top,
                                  &DestRect, // Bounding rectangle
                                  Mix);
    }

cleanup:
    if (FillBrushObj)
        BRUSHOBJ_UnlockBrush(FillBrushObj);

    if (PenBrushObj)
        PENOBJ_UnlockPen(PenBrushObj);

    if (BitmapObj)
        BITMAPOBJ_UnlockBitmap(BitmapObj);

    /* Move current position in DC?
       MSDN: The current position is neither used nor updated by Rectangle. */

    return ret;
}

BOOL
STDCALL
NtGdiRectangle(HDC  hDC,
               int  LeftRect,
               int  TopRect,
               int  RightRect,
               int  BottomRect)
{
    DC   *dc;
    BOOL ret; // default to failure

    dc = DC_LockDc(hDC);
    if (!dc)
    {
        SetLastWin32Error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (dc->DC_Type == DC_TYPE_INFO)
    {
        DC_UnlockDc(dc);
        /* Yes, Windows really returns TRUE in this case */
        return TRUE;
    }

    ret = IntRectangle ( dc, LeftRect, TopRect, RightRect, BottomRect );
    DC_UnlockDc ( dc );

    return ret;
}


BOOL
FASTCALL
IntRoundRect(
    PDC  dc,
    int  Left,
    int  Top,
    int  Right,
    int  Bottom,
    int  xCurveDiameter,
    int  yCurveDiameter)
{
    PDC_ATTR Dc_Attr;
    PGDIBRUSHOBJ   PenBrushObj;
    RECTL RectBounds;
    LONG PenWidth, PenOrigWidth;
    BOOL ret = TRUE; // default to success

    ASSERT ( dc ); // caller's responsibility to set this up

    if ( PATH_IsPathOpen(dc->DcLevel) )
        return PATH_RoundRect ( dc, Left, Top, Right, Bottom,
                                xCurveDiameter, yCurveDiameter );

    if ((Left == Right) || (Top == Bottom)) return TRUE;

    xCurveDiameter = max(abs( xCurveDiameter ), 1);
    yCurveDiameter = max(abs( yCurveDiameter ), 1);

    if (Right < Left)
    {
       INT tmp = Right; Right = Left; Left = tmp;
    }
    if (Bottom < Top)
    {
       INT tmp = Bottom; Bottom = Top; Top = tmp;
    }

    Dc_Attr = dc->pDc_Attr;
    if(!Dc_Attr) Dc_Attr = &dc->Dc_Attr;

    PenBrushObj = PENOBJ_LockPen(Dc_Attr->hpen);
    if (!PenBrushObj)
    {
        /* Nothing to do, as we don't have a bitmap */
        SetLastWin32Error(ERROR_INTERNAL_ERROR);
        return FALSE;
    }

    PenOrigWidth = PenWidth = PenBrushObj->ptPenWidth.x;
    if (PenBrushObj->ulPenStyle == PS_NULL) PenWidth = 0;

    if (PenBrushObj->ulPenStyle == PS_INSIDEFRAME)
    {
       if (2*PenWidth > (Right - Left)) PenWidth = (Right -Left + 1)/2;
       if (2*PenWidth > (Bottom - Top)) PenWidth = (Bottom -Top + 1)/2;
       Left   += PenWidth / 2;
       Right  -= (PenWidth - 1) / 2;
       Top    += PenWidth / 2;
       Bottom -= (PenWidth - 1) / 2;
    }

    if (!PenWidth) PenWidth = 1;
    PenBrushObj->ptPenWidth.x = PenWidth;  

    RectBounds.left = Left;
    RectBounds.top = Top;
    RectBounds.right = Right;
    RectBounds.bottom = Bottom;

    IntLPtoDP(dc, (LPPOINT)&RectBounds, 2);

    RectBounds.left   += dc->ptlDCOrig.x;
    RectBounds.top    += dc->ptlDCOrig.y;
    RectBounds.right  += dc->ptlDCOrig.x;
    RectBounds.bottom += dc->ptlDCOrig.y;

    ret = IntFillRoundRect( dc,
               RectBounds.left,
                RectBounds.top,
              RectBounds.right,
             RectBounds.bottom,
                xCurveDiameter,
                yCurveDiameter);
    if (ret)
       ret = IntDrawRoundRect( dc,
                  RectBounds.left,
                   RectBounds.top,
                 RectBounds.right,
                RectBounds.bottom,
                   xCurveDiameter,
                   yCurveDiameter,
                   PenBrushObj);

    PenBrushObj->ptPenWidth.x = PenOrigWidth;
    PENOBJ_UnlockPen(PenBrushObj);
    return ret;
}

BOOL
STDCALL
NtGdiRoundRect(
    HDC  hDC,
    int  LeftRect,
    int  TopRect,
    int  RightRect,
    int  BottomRect,
    int  Width,
    int  Height)
{
    DC   *dc = DC_LockDc(hDC);
    BOOL  ret = FALSE; /* default to failure */

    DPRINT("NtGdiRoundRect(0x%x,%i,%i,%i,%i,%i,%i)\n",hDC,LeftRect,TopRect,RightRect,BottomRect,Width,Height);
    if ( !dc )
    {
        DPRINT1("NtGdiRoundRect() - hDC is invalid\n");
        SetLastWin32Error(ERROR_INVALID_HANDLE);
    }
    else if (dc->DC_Type == DC_TYPE_INFO)
    {
        DC_UnlockDc(dc);
        /* Yes, Windows really returns TRUE in this case */
        ret = TRUE;
    }
    else
    {
        ret = IntRoundRect ( dc, LeftRect, TopRect, RightRect, BottomRect, Width, Height );
        DC_UnlockDc ( dc );
    }

    return ret;
}

BOOL FASTCALL
IntGdiGradientFill(
    DC *dc,
    PTRIVERTEX pVertex,
    ULONG uVertex,
    PVOID pMesh,
    ULONG uMesh,
    ULONG ulMode)
{
    BITMAPOBJ *BitmapObj;
    PPALGDI PalDestGDI;
    XLATEOBJ *XlateObj;
    RECTL Extent;
    POINTL DitherOrg;
    ULONG Mode, i;
    BOOL Ret;
    HPALETTE hDestPalette;

    ASSERT(dc);
    ASSERT(pVertex);
    ASSERT(uVertex);
    ASSERT(pMesh);
    ASSERT(uMesh);

    /* check parameters */
    if (ulMode & GRADIENT_FILL_TRIANGLE)
    {
        PGRADIENT_TRIANGLE tr = (PGRADIENT_TRIANGLE)pMesh;

        for (i = 0; i < uMesh; i++, tr++)
        {
            if (tr->Vertex1 >= uVertex ||
                    tr->Vertex2 >= uVertex ||
                    tr->Vertex3 >= uVertex)
            {
                SetLastWin32Error(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
        }
    }
    else
    {
        PGRADIENT_RECT rc = (PGRADIENT_RECT)pMesh;
        for (i = 0; i < uMesh; i++, rc++)
        {
            if (rc->UpperLeft >= uVertex || rc->LowerRight >= uVertex)
            {
                SetLastWin32Error(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
        }
    }

    /* calculate extent */
    Extent.left = Extent.right = pVertex->x;
    Extent.top = Extent.bottom = pVertex->y;
    for (i = 0; i < uVertex; i++)
    {
        Extent.left = min(Extent.left, (pVertex + i)->x);
        Extent.right = max(Extent.right, (pVertex + i)->x);
        Extent.top = min(Extent.top, (pVertex + i)->y);
        Extent.bottom = max(Extent.bottom, (pVertex + i)->y);
    }

    DitherOrg.x = dc->ptlDCOrig.x;
    DitherOrg.y = dc->ptlDCOrig.y;
    Extent.left += DitherOrg.x;
    Extent.right += DitherOrg.x;
    Extent.top += DitherOrg.y;
    Extent.bottom += DitherOrg.y;

    BitmapObj = BITMAPOBJ_LockBitmap(dc->w.hBitmap);
    /* FIXME - BitmapObj can be NULL!!! Don't assert but handle this case gracefully! */
    ASSERT(BitmapObj);

    hDestPalette = BitmapObj->hDIBPalette;
    if (!hDestPalette) hDestPalette = pPrimarySurface->DevInfo.hpalDefault;

    PalDestGDI = PALETTE_LockPalette(hDestPalette);
    if (PalDestGDI)
    {
        Mode = PalDestGDI->Mode;
        PALETTE_UnlockPalette(PalDestGDI);
    }
    else
        Mode = PAL_RGB;

    XlateObj = (XLATEOBJ*)IntEngCreateXlate(Mode, PAL_RGB, hDestPalette, NULL);
    ASSERT(XlateObj);

    Ret = IntEngGradientFill(&BitmapObj->SurfObj,
                             dc->CombinedClip,
                             XlateObj,
                             pVertex,
                             uVertex,
                             pMesh,
                             uMesh,
                             &Extent,
                             &DitherOrg,
                             ulMode);

    BITMAPOBJ_UnlockBitmap(BitmapObj);
    EngDeleteXlate(XlateObj);

    return Ret;
}

BOOL
STDCALL
NtGdiGradientFill(
    HDC hdc,
    PTRIVERTEX pVertex,
    ULONG uVertex,
    PVOID pMesh,
    ULONG uMesh,
    ULONG ulMode)
{
    DC *dc;
    BOOL Ret;
    PTRIVERTEX SafeVertex;
    PVOID SafeMesh;
    ULONG SizeMesh;
    NTSTATUS Status = STATUS_SUCCESS;

    dc = DC_LockDc(hdc);
    if (!dc)
    {
        SetLastWin32Error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (dc->DC_Type == DC_TYPE_INFO)
    {
        DC_UnlockDc(dc);
        /* Yes, Windows really returns TRUE in this case */
        return TRUE;
    }
    if (!pVertex || !uVertex || !pMesh || !uMesh)
    {
        DC_UnlockDc(dc);
        SetLastWin32Error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    switch (ulMode)
    {
        case GRADIENT_FILL_RECT_H:
        case GRADIENT_FILL_RECT_V:
            SizeMesh = uMesh * sizeof(GRADIENT_RECT);
            break;
        case GRADIENT_FILL_TRIANGLE:
            SizeMesh = uMesh * sizeof(TRIVERTEX);
            break;
        default:
            DC_UnlockDc(dc);
            SetLastWin32Error(ERROR_INVALID_PARAMETER);
            return FALSE;
    }

    _SEH_TRY
    {
        ProbeForRead(pVertex,
                     uVertex * sizeof(TRIVERTEX),
                     1);
        ProbeForRead(pMesh,
                     SizeMesh,
                     1);
    }
    _SEH_HANDLE
    {
        Status = _SEH_GetExceptionCode();
    }
    _SEH_END;

    if (!NT_SUCCESS(Status))
    {
        DC_UnlockDc(dc);
        SetLastWin32Error(Status);
        return FALSE;
    }

    if (!(SafeVertex = ExAllocatePoolWithTag(PagedPool, (uVertex * sizeof(TRIVERTEX)) + SizeMesh, TAG_SHAPE)))
    {
        DC_UnlockDc(dc);
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    SafeMesh = (PTRIVERTEX)(SafeVertex + uVertex);

    _SEH_TRY
    {
        /* pointers were already probed! */
        RtlCopyMemory(SafeVertex,
                      pVertex,
                      uVertex * sizeof(TRIVERTEX));
        RtlCopyMemory(SafeMesh,
                      pMesh,
                      SizeMesh);
    }
    _SEH_HANDLE
    {
        Status = _SEH_GetExceptionCode();
    }
    _SEH_END;

    if (!NT_SUCCESS(Status))
    {
        DC_UnlockDc(dc);
        ExFreePoolWithTag(SafeVertex, TAG_SHAPE);
        SetLastNtError(Status);
        return FALSE;
    }

    Ret = IntGdiGradientFill(dc, SafeVertex, uVertex, SafeMesh, uMesh, ulMode);

    DC_UnlockDc(dc);
    ExFreePool(SafeVertex);
    return Ret;
}

BOOL STDCALL
NtGdiExtFloodFill(
    HDC  hDC,
    INT  XStart,
    INT  YStart,
    COLORREF  Color,
    UINT  FillType)
{
    DPRINT1("FIXME: NtGdiExtFloodFill is UNIMPLEMENTED\n");

    /* lie and say we succeded */
    return TRUE;
}

/* EOF */
