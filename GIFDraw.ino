// GIFDraw is called by AnimatedGIF library frame to screen

#define DISPLAY_WIDTH  tft.width()
#define DISPLAY_HEIGHT tft.height()
#define BUFFER_SIZE 280            // Optimum is >= GIF width or integral division of width

uint16_t usTemp[1][BUFFER_SIZE];    // Global to support DMA use
  
// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth, iCount;

  // Calculate centering offsets
  static int xOffset = (DISPLAY_WIDTH - pDraw->iWidth) / 2;
  static int yOffset = (DISPLAY_HEIGHT - pDraw->iHeight) / 2;

  // Apply offsets to position
  int centeredX = pDraw->iX + xOffset;
  int centeredY = pDraw->iY + yOffset + pDraw->y; // current line

  // Display bounds check and cropping (with centering)
  iWidth = pDraw->iWidth;
  if (centeredX + iWidth > DISPLAY_WIDTH)
    iWidth = DISPLAY_WIDTH - centeredX;
  if (centeredX < 0) {
    iWidth += centeredX;
    centeredX = 0;
  }

  usPalette = pDraw->pPalette;
  if (centeredY >= DISPLAY_HEIGHT || centeredX >= DISPLAY_WIDTH || iWidth < 1)
    return;

  // Old image disposal
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < iWidth)
    {
      c = ucTransparent - 1;
      d = &usTemp[0][0];
      while (c != ucTransparent && s < pEnd && iCount < BUFFER_SIZE )
      {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        }
        else // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      } // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        // ONLY CHANGE #4 - Replace pDraw->iX with centeredX, y with centeredY
        tft.setAddrWindow(centeredX + x, centeredY, iCount, 1);
        tft.pushPixels(usTemp, iCount);
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent)
          x++;
        else
          s--;
      }
    }
  }
  else
  {
    s = pDraw->pPixels;

    // Unroll the first pass to boost DMA performance
    if (iWidth <= BUFFER_SIZE)
      for (iCount = 0; iCount < iWidth; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];
    else
      for (iCount = 0; iCount < BUFFER_SIZE; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];

    // ONLY CHANGE #5 - Replace pDraw->iX with centeredX, y with centeredY
    tft.setAddrWindow(centeredX, centeredY, iWidth, 1);
    tft.pushPixels(&usTemp[0][0], iCount);

    iWidth -= iCount;
    // Loop if pixel buffer smaller than width
    while (iWidth > 0)
    {
      if (iWidth <= BUFFER_SIZE)
        for (iCount = 0; iCount < iWidth; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];
      else
        for (iCount = 0; iCount < BUFFER_SIZE; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];

      tft.pushPixels(&usTemp[0][0], iCount);
      iWidth -= iCount;
    }
  }
} /* GIFDraw() */
