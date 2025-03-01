/*
 * Copyright 2023 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm/gm.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkStream.h"
#include "include/core/SkString.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkTypeface_fontations.h"
#include "tools/Resources.h"

namespace skiagm {

namespace {
const SkScalar kTextSizes[] = {12, 18, 30, 120};
const char kReportFontName[] = "fonts/Roboto-Regular.ttf";
const SkScalar kDumpFontSize = 20.0f;

// TODO(drott): Test these dumps is in a unit test instead of dumping them to GM surface.
void dumpToCanvas(SkCanvas* canvas, SkString text, sk_sp<SkTypeface> reportTypeface) {
    canvas->drawSimpleText(text.c_str(),
                           text.size() - 1,
                           SkTextEncoding::kUTF8,
                           0,
                           0,
                           SkFont(reportTypeface, kDumpFontSize),
                           SkPaint());
}

void dumpLocalizedStrings(SkCanvas* canvas,
                          sk_sp<SkTypeface> typeface,
                          sk_sp<SkTypeface> reportTypeface) {
    auto family_names = typeface->createFamilyNameIterator();
    SkTypeface::LocalizedString famName;
    SkString localizedName;
    while (family_names->next(&famName)) {
        localizedName.printf(
                "Name: %s Language: %s\n", famName.fString.c_str(), famName.fLanguage.c_str());
        dumpToCanvas(canvas, localizedName, reportTypeface);
        canvas->translate(0, kDumpFontSize * 1.2);
    }
    family_names->unref();
}

void dumpGlyphCount(SkCanvas* canvas,
                    sk_sp<SkTypeface> typeface,
                    sk_sp<SkTypeface> reportTypeface) {
    SkString glyphCount;
    glyphCount.printf("Num glyphs: %d\n", typeface->countGlyphs());
    dumpToCanvas(canvas, glyphCount, reportTypeface);
}

void dumpFamilyAndPostscriptName(SkCanvas* canvas,
                                 sk_sp<SkTypeface> typeface,
                                 sk_sp<SkTypeface> reportTypeface) {
    SkString name;
    typeface->getFamilyName(&name);
    SkString nameDump;
    nameDump.printf("Family name: %s\n", name.c_str());
    dumpToCanvas(canvas, nameDump, reportTypeface);

    if (typeface->getPostScriptName(&name)) {
        canvas->translate(0, kDumpFontSize * 1.2);
        nameDump.printf("PS Name: %s\n", name.c_str());
        dumpToCanvas(canvas, nameDump, reportTypeface);
    } else {
        canvas->translate(0, kDumpFontSize * 1.2);
        nameDump.printf("No Postscript name.");
        dumpToCanvas(canvas, nameDump, reportTypeface);
    }
}

}  // namespace

class FontationsTypefaceGM : public GM {
public:
    FontationsTypefaceGM(const char* testName,
                         const char* testFontFilename,
                         std::initializer_list<SkFontArguments::VariationPosition::Coordinate>
                                 specifiedVariations)
            : fTestName(testName), fTestFontFilename(testFontFilename) {
        this->setBGColor(SK_ColorWHITE);
        fVariationPosition.coordinateCount = specifiedVariations.size();
        fCoordinates = std::make_unique<SkFontArguments::VariationPosition::Coordinate[]>(
                specifiedVariations.size());
        for (size_t i = 0; i < specifiedVariations.size(); ++i) {
            fCoordinates[i] = std::data(specifiedVariations)[i];
        }

        fVariationPosition.coordinates = fCoordinates.get();
    }

protected:
    void onOnceBeforeDraw() override {
        fTestTypeface = SkTypeface_Make_Fontations(
                GetResourceAsStream(fTestFontFilename),
                SkFontArguments().setVariationDesignPosition(fVariationPosition));
        fReportTypeface =
                SkTypeface_Make_Fontations(GetResourceAsStream(kReportFontName), SkFontArguments());
    }

    SkString onShortName() override {
        return SkStringPrintf("typeface_fontations_%s", fTestName.c_str());
    }

    SkISize onISize() override { return SkISize::Make(400, 200); }

    DrawResult onDraw(SkCanvas* canvas, SkString* errorMsg) override {
        SkPaint paint;
        paint.setColor(SK_ColorBLACK);

        if (!fTestTypeface) {
            *errorMsg = "Unable to initialize typeface.";
            return DrawResult::kSkip;
        }

        SkFont font(fTestTypeface);
        const char32_t testText[] = U"abc";
        size_t testTextBytesize = std::char_traits<char32_t>::length(testText) * sizeof(char32_t);
        SkScalar x = 100;
        SkScalar y = 150;

        for (SkScalar textSize : kTextSizes) {
            font.setSize(textSize);
            y += font.getSpacing();

            /* Draw origin marker as a green dot. */
            paint.setColor(SK_ColorGREEN);
            canvas->drawRect(SkRect::MakeXYWH(x, y, 2, 2), paint);
            paint.setColor(SK_ColorBLACK);

            canvas->drawSimpleText(
                    testText, testTextBytesize, SkTextEncoding::kUTF32, x, y, font, paint);
        }

        canvas->translate(100, 470);
        dumpGlyphCount(canvas, fTestTypeface, fReportTypeface);
        canvas->translate(0, kDumpFontSize * 1.2);
        dumpLocalizedStrings(canvas, fTestTypeface, fReportTypeface);
        canvas->translate(0, kDumpFontSize * 1.2);
        dumpFamilyAndPostscriptName(canvas, fTestTypeface, fReportTypeface);

        return DrawResult::kOk;
    }

private:
    using INHERITED = GM;

    const SkString fTestName;
    const char* fTestFontFilename;
    sk_sp<SkTypeface> fTestTypeface;
    sk_sp<SkTypeface> fReportTypeface;
    SkFontArguments::VariationPosition fVariationPosition;
    std::unique_ptr<SkFontArguments::VariationPosition::Coordinate[]> fCoordinates;
};

namespace {
SkFourByteTag constexpr operator"" _t(const char* tagName, size_t size) {
    SkASSERT(size == 4);
    return SkSetFourByteTag(tagName[0], tagName[1], tagName[2], tagName[3]);
}
}  // namespace
DEF_GM(return new FontationsTypefaceGM("roboto", "fonts/Roboto-Regular.ttf", {});)
DEF_GM(return new FontationsTypefaceGM("distortable_light",
                                       "fonts/Distortable.ttf",
                                       {{"wght"_t, 0.5f}});)
DEF_GM(return new FontationsTypefaceGM("distortable_bold",
                                       "fonts/Distortable.ttf",
                                       {{"wght"_t, 2.0f}});)

}  // namespace skiagm
