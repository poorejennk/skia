/*
 * Copyright 2023 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/effects/colorfilters/SkRuntimeColorFilter.h"

#include "include/core/SkAlphaType.h"
#include "include/core/SkCapabilities.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkData.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkScalar.h"
#include "include/core/SkString.h"
#include "include/effects/SkLumaColorFilter.h"
#include "include/effects/SkOverdrawColorFilter.h"
#include "include/effects/SkRuntimeEffect.h"
#include "include/private/SkColorData.h"
#include "include/private/SkSLSampleUsage.h"
#include "include/private/base/SkAssert.h"
#include "include/private/base/SkDebug.h"
#include "include/private/base/SkTArray.h"
#include "src/core/SkEffectPriv.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkRuntimeEffectPriv.h"
#include "src/core/SkWriteBuffer.h"
#include "src/effects/colorfilters/SkColorFilterBase.h"
#include "src/shaders/SkShaderBase.h"
#include "src/sksl/codegen/SkSLRasterPipelineBuilder.h"

#include <string>
#include <utility>

#if !defined(SK_ENABLE_SKSL)
#error This only be compiled if SKSL is enabled. See _none.cpp for the non-SKSL version.
#endif

#if defined(SK_ENABLE_SKVM)
#include "src/core/SkFilterColorProgram.h"
#endif

#if defined(SK_GRAPHITE)
#include "src/gpu/graphite/KeyContext.h"
#include "src/gpu/graphite/KeyHelpers.h"
#include "src/gpu/graphite/PaintParamsKey.h"
#endif

class SkColorSpace;

SkRuntimeColorFilter::SkRuntimeColorFilter(sk_sp<SkRuntimeEffect> effect,
                                           sk_sp<const SkData> uniforms,
                                           SkSpan<SkRuntimeEffect::ChildPtr> children)
        : fEffect(std::move(effect))
        , fUniforms(std::move(uniforms))
        , fChildren(children.begin(), children.end()) {}

#if defined(SK_GRAPHITE)
void SkRuntimeColorFilter::addToKey(const skgpu::graphite::KeyContext& keyContext,
                                    skgpu::graphite::PaintParamsKeyBuilder* builder,
                                    skgpu::graphite::PipelineDataGatherer* gatherer) const {
    using namespace skgpu::graphite;

    sk_sp<const SkData> uniforms = SkRuntimeEffectPriv::TransformUniforms(
            fEffect->uniforms(), fUniforms, keyContext.dstColorInfo().colorSpace());
    SkASSERT(uniforms);

    RuntimeEffectBlock::BeginBlock(keyContext, builder, gatherer, {fEffect, std::move(uniforms)});

    SkRuntimeEffectPriv::AddChildrenToKey(
            fChildren, fEffect->children(), keyContext, builder, gatherer);

    builder->endBlock();
}
#endif

bool SkRuntimeColorFilter::appendStages(const SkStageRec& rec, bool) const {
#ifdef SK_ENABLE_SKSL_IN_RASTER_PIPELINE
    if (!SkRuntimeEffectPriv::CanDraw(SkCapabilities::RasterBackend().get(), fEffect.get())) {
        // SkRP has support for many parts of #version 300 already, but for now, we restrict its
        // usage in runtime effects to just #version 100.
        return false;
    }
    if (const SkSL::RP::Program* program = fEffect->getRPProgram(/*debugTrace=*/nullptr)) {
        SkSpan<const float> uniforms =
                SkRuntimeEffectPriv::UniformsAsSpan(fEffect->uniforms(),
                                                    fUniforms,
                                                    /*alwaysCopyIntoAlloc=*/false,
                                                    rec.fDstCS,
                                                    rec.fAlloc);
        SkShaderBase::MatrixRec matrix(SkMatrix::I());
        matrix.markCTMApplied();
        RuntimeEffectRPCallbacks callbacks(rec, matrix, fChildren, fEffect->fSampleUsages);
        bool success = program->appendStages(rec.fPipeline, rec.fAlloc, &callbacks, uniforms);
        return success;
    }
#endif
    return false;
}

#if defined(SK_ENABLE_SKVM)
skvm::Color SkRuntimeColorFilter::onProgram(skvm::Builder* p,
                                            skvm::Color c,
                                            const SkColorInfo& colorInfo,
                                            skvm::Uniforms* uniforms,
                                            SkArenaAlloc* alloc) const {
    SkASSERT(SkRuntimeEffectPriv::CanDraw(SkCapabilities::RasterBackend().get(), fEffect.get()));

    sk_sp<const SkData> inputs = SkRuntimeEffectPriv::TransformUniforms(
            fEffect->uniforms(), fUniforms, colorInfo.colorSpace());
    SkASSERT(inputs);

    SkShaderBase::MatrixRec mRec(SkMatrix::I());
    mRec.markTotalMatrixInvalid();
    RuntimeEffectVMCallbacks callbacks(p, uniforms, alloc, fChildren, mRec, c, colorInfo);
    std::vector<skvm::Val> uniform =
            SkRuntimeEffectPriv::MakeSkVMUniforms(p, uniforms, fEffect->uniformSize(), *inputs);

    // There should be no way for the color filter to use device coords, but we need to supply
    // something. (Uninitialized values can trigger asserts in skvm::Builder).
    skvm::Coord zeroCoord = {p->splat(0.0f), p->splat(0.0f)};
    return SkSL::ProgramToSkVM(*fEffect->fBaseProgram,
                               fEffect->fMain,
                               p,
                               /*debugTrace=*/nullptr,
                               SkSpan(uniform),
                               /*device=*/zeroCoord,
                               /*local=*/zeroCoord,
                               c,
                               c,
                               &callbacks);
}
#endif

SkPMColor4f SkRuntimeColorFilter::onFilterColor4f(const SkPMColor4f& color,
                                                  SkColorSpace* dstCS) const {
#if defined(SK_ENABLE_SKVM)
    // Get the generic program for filtering a single color
    if (const SkFilterColorProgram* program = fEffect->getFilterColorProgram()) {
        // Get our specific uniform values
        sk_sp<const SkData> inputs =
                SkRuntimeEffectPriv::TransformUniforms(fEffect->uniforms(), fUniforms, dstCS);
        SkASSERT(inputs);

        auto evalChild = [&](int index, SkPMColor4f inColor) {
            const auto& child = fChildren[index];

            // SkFilterColorProgram::Make has guaranteed that any children will be color filters.
            SkASSERT(!child.shader());
            SkASSERT(!child.blender());
            if (SkColorFilter* colorFilter = child.colorFilter()) {
                return as_CFB(colorFilter)->onFilterColor4f(inColor, dstCS);
            }
            return inColor;
        };

        return program->eval(color, inputs->data(), evalChild);
    }
#endif
    // We were unable to build a cached (per-effect) program. Use the base-class fallback,
    // which builds a program for the specific filter instance.
    return SkColorFilterBase::onFilterColor4f(color, dstCS);
}

bool SkRuntimeColorFilter::onIsAlphaUnchanged() const {
#ifdef SK_ENABLE_SKSL_IN_RASTER_PIPELINE
    return fEffect->isAlphaUnchanged();
#else
    return fEffect->getFilterColorProgram() && fEffect->isAlphaUnchanged();
#endif
}

void SkRuntimeColorFilter::flatten(SkWriteBuffer& buffer) const {
    buffer.writeString(fEffect->source().c_str());
    buffer.writeDataAsByteArray(fUniforms.get());
    SkRuntimeEffectPriv::WriteChildEffects(buffer, fChildren);
}

SkRuntimeEffect* SkRuntimeColorFilter::asRuntimeEffect() const { return fEffect.get(); }

sk_sp<SkFlattenable> SkRuntimeColorFilter::CreateProc(SkReadBuffer& buffer) {
    if (!buffer.validate(buffer.allowSkSL())) {
        return nullptr;
    }

    SkString sksl;
    buffer.readString(&sksl);
    sk_sp<SkData> uniforms = buffer.readByteArrayAsData();

    auto effect = SkMakeCachedRuntimeEffect(SkRuntimeEffect::MakeForColorFilter, std::move(sksl));
#if !SK_LENIENT_SKSL_DESERIALIZATION
    if (!buffer.validate(effect != nullptr)) {
        return nullptr;
    }
#endif

    skia_private::STArray<4, SkRuntimeEffect::ChildPtr> children;
    if (!SkRuntimeEffectPriv::ReadChildEffects(buffer, effect.get(), &children)) {
        return nullptr;
    }

#if SK_LENIENT_SKSL_DESERIALIZATION
    if (!effect) {
        SkDebugf("Serialized SkSL failed to compile. Ignoring/dropping SkSL color filter.\n");
        return nullptr;
    }
#endif

    return effect->makeColorFilter(std::move(uniforms), SkSpan(children));
}

/////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkColorFilter> SkColorFilters::Lerp(float weight, sk_sp<SkColorFilter> cf0,
                                                        sk_sp<SkColorFilter> cf1) {
    if (!cf0 && !cf1) {
        return nullptr;
    }
    if (SkScalarIsNaN(weight)) {
        return nullptr;
    }

    if (cf0 == cf1) {
        return cf0; // or cf1
    }

    if (weight <= 0) {
        return cf0;
    }
    if (weight >= 1) {
        return cf1;
    }

    static const SkRuntimeEffect* effect =
            SkMakeCachedRuntimeEffect(SkRuntimeEffect::MakeForColorFilter,
                                      "uniform colorFilter cf0;"
                                      "uniform colorFilter cf1;"
                                      "uniform half weight;"
                                      "half4 main(half4 color) {"
                                      "return mix(cf0.eval(color), cf1.eval(color), weight);"
                                      "}")
                    .release();
    SkASSERT(effect);

    sk_sp<SkColorFilter> inputs[] = {cf0,cf1};
    return effect->makeColorFilter(SkData::MakeWithCopy(&weight, sizeof(weight)),
                                   inputs, std::size(inputs));
}

sk_sp<SkColorFilter> SkLumaColorFilter::Make() {
    static const SkRuntimeEffect* effect = SkMakeCachedRuntimeEffect(
        SkRuntimeEffect::MakeForColorFilter,
        "half4 main(half4 inColor) {"
            "return saturate(dot(half3(0.2126, 0.7152, 0.0722), inColor.rgb)).000r;"
        "}"
    ).release();
    SkASSERT(effect);

    return effect->makeColorFilter(SkData::MakeEmpty());
}

sk_sp<SkColorFilter> SkOverdrawColorFilter::MakeWithSkColors(const SkColor colors[kNumColors]) {
    static const SkRuntimeEffect* effect = SkMakeCachedRuntimeEffect(
        SkRuntimeEffect::MakeForColorFilter,
        "uniform half4 color0, color1, color2, color3, color4, color5;"

        "half4 main(half4 color) {"
            "half alpha = 255.0 * color.a;"
            "return alpha < 0.5 ? color0"
                 ": alpha < 1.5 ? color1"
                 ": alpha < 2.5 ? color2"
                 ": alpha < 3.5 ? color3"
                 ": alpha < 4.5 ? color4 : color5;"
        "}"
    ).release();

    if (effect) {
        auto data = SkData::MakeUninitialized(kNumColors * sizeof(SkPMColor4f));
        SkPMColor4f* premul = (SkPMColor4f*)data->writable_data();
        for (int i = 0; i < kNumColors; ++i) {
            premul[i] = SkColor4f::FromColor(colors[i]).premul();
        }
        return effect->makeColorFilter(std::move(data));
    }
    return nullptr;
}

