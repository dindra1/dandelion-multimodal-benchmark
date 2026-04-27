#pragma once
// Minimal Tesseract C API declarations — avoids depending on the full
// Tesseract headers while still linking against libtesseract-5.dll.
#ifdef __cplusplus
extern "C" {
#endif

typedef struct TessBaseAPI TessBaseAPI;
typedef struct Pix PIX; // opaque Leptonica type

typedef enum {
    PSM_AUTO_OSD  = 0,
    PSM_AUTO      = 3,
    PSM_SINGLE_BLOCK = 6,
} TessPageSegMode;

TessBaseAPI* TessBaseAPICreate(void);
int  TessBaseAPIInit3(TessBaseAPI*, const char* datapath, const char* language);
void TessBaseAPISetImage(TessBaseAPI*, const unsigned char* imagedata,
                         int width, int height,
                         int bytes_per_pixel, int bytes_per_line);
void TessBaseAPISetPageSegMode(TessBaseAPI*, TessPageSegMode mode);
char* TessBaseAPIGetUTF8Text(TessBaseAPI*);
void TessBaseAPIClear(TessBaseAPI*);
void TessBaseAPIEnd(TessBaseAPI*);
void TessBaseAPIDelete(TessBaseAPI*);
void TessDeleteText(char* text);

#ifdef __cplusplus
}
#endif
