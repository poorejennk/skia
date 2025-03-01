/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm/gm.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSize.h"
#include "include/core/SkString.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypes.h"
#include "include/effects/SkImageFilters.h"

namespace skiagm {

// This GM reproduces the issue in crbug.com/472795. The SkImageSource image
// is shifted for high quality mode between cpu and gpu.
class ImageSourceGM : public GM {
public:
    ImageSourceGM(const char* suffix, const SkSamplingOptions& sampling)
        : fSuffix(suffix), fSampling(sampling) {
        this->setBGColor(0xFFFFFFFF);
    }

protected:
    SkString onShortName() override {
        SkString name("imagesrc2_");
        name.append(fSuffix);
        return name;
    }

    SkISize onISize() override { return SkISize::Make(256, 256); }

    // Create an image with high frequency vertical stripes
    void onOnceBeforeDraw() override {
        constexpr SkPMColor gColors[] = {
            SK_ColorRED,     SK_ColorGRAY,
            SK_ColorGREEN,   SK_ColorGRAY,
            SK_ColorBLUE,    SK_ColorGRAY,
            SK_ColorCYAN,    SK_ColorGRAY,
            SK_ColorMAGENTA, SK_ColorGRAY,
            SK_ColorYELLOW,  SK_ColorGRAY,
            SK_ColorWHITE,   SK_ColorGRAY,
        };

        auto surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kImageSize, kImageSize)));
        SkCanvas* canvas = surface->getCanvas();

        int curColor = 0;

        for (int x = 0; x < kImageSize; x += 3) {
            SkRect r = SkRect::MakeXYWH(SkIntToScalar(x), SkIntToScalar(0),
                                        SkIntToScalar(3), SkIntToScalar(kImageSize));
            SkPaint p;
            p.setColor(gColors[curColor]);
            canvas->drawRect(r, p);

            curColor = (curColor+1) % std::size(gColors);
        }

        fImage = surface->makeImageSnapshot();
    }

    void onDraw(SkCanvas* canvas) override {
        const SkRect srcRect = SkRect::MakeLTRB(0, 0,
                                                SkIntToScalar(kImageSize),
                                                SkIntToScalar(kImageSize));
        const SkRect dstRect = SkRect::MakeLTRB(0.75f, 0.75f, 225.75f, 225.75f);

        SkPaint p;
        p.setImageFilter(SkImageFilters::Image(fImage, srcRect, dstRect, fSampling));

        canvas->saveLayer(nullptr, &p);
        canvas->restore();
    }

private:
    inline static constexpr int kImageSize = 503;

    SkString          fSuffix;
    SkSamplingOptions fSampling;
    sk_sp<SkImage>    fImage;

    using INHERITED = GM;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM(return new ImageSourceGM("none", SkSamplingOptions());)
DEF_GM(return new ImageSourceGM("low", SkSamplingOptions(SkFilterMode::kLinear));)
DEF_GM(return new ImageSourceGM("med", SkSamplingOptions(SkFilterMode::kLinear,
                                                         SkMipmapMode::kLinear));)
DEF_GM(return new ImageSourceGM("high", SkSamplingOptions({1/3.0f, 1/3.0f}));)
}  // namespace skiagm
