/*
 * Copyright 2022 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_WGSLCODEGENERATOR
#define SKSL_WGSLCODEGENERATOR

#include "include/core/SkSpan.h"
#include "include/private/SkSLDefines.h"
#include "src/core/SkTHash.h"
#include "src/sksl/SkSLOperator.h"
#include "src/sksl/codegen/SkSLCodeGenerator.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace sknonstd {
template <typename T> struct is_bitmask_enum;
}  // namespace sknonstd

namespace SkSL {

class AnyConstructor;
class BinaryExpression;
class Block;
class Context;
class ConstructorCompound;
class ConstructorDiagonalMatrix;
class ConstructorMatrixResize;
class DoStatement;
class Expression;
struct Field;
class FieldAccess;
class ForStatement;
class FunctionCall;
class FunctionDeclaration;
class FunctionDefinition;
class GlobalVarDeclaration;
class IfStatement;
class IndexExpression;
class Literal;
class MemoryLayout;
class OutputStream;
class Position;
class PostfixExpression;
class PrefixExpression;
class ProgramElement;
class ReturnStatement;
class Statement;
class StructDefinition;
class Swizzle;
class TernaryExpression;
class Type;
class VarDeclaration;
class VariableReference;
struct Modifiers;
struct Program;

/**
 * Convert a Program into WGSL code.
 */
class WGSLCodeGenerator : public CodeGenerator {
public:
    // See https://www.w3.org/TR/WGSL/#builtin-values
    enum class Builtin {
        // Vertex stage:
        kVertexIndex,    // input
        kInstanceIndex,  // input
        kPosition,       // output, fragment stage input

        // Fragment stage:
        kFrontFacing,  // input
        kSampleIndex,  // input
        kFragDepth,    // output
        kSampleMask,   // input, output

        // Compute stage:
        kLocalInvocationId,     // input
        kLocalInvocationIndex,  // input
        kGlobalInvocationId,    // input
        kWorkgroupId,           // input
        kNumWorkgroups,         // input
    };

    // Represents a function's dependencies that are not accessible in global scope. For instance,
    // pipeline stage input and output parameters must be passed in as an argument.
    //
    // This is a bitmask enum.
    enum class FunctionDependencies : uint8_t {
        kNone = 0,
        kPipelineInputs = 1,
        kPipelineOutputs = 2,
    };

    // Variable declarations can be terminated by:
    //   - comma (","), e.g. in struct member declarations or function parameters
    //   - semicolon (";"), e.g. in function scope variables
    // A "none" option is provided to skip the delimiter when not needed, e.g. at the end of a list
    // of declarations.
    enum class Delimiter {
        kComma,
        kSemicolon,
        kNone,
    };

    struct ProgramRequirements {
        using DepsMap = skia_private::THashMap<const FunctionDeclaration*, FunctionDependencies>;

        ProgramRequirements() = default;
        ProgramRequirements(DepsMap dependencies, bool mainNeedsCoordsArgument)
                : dependencies(std::move(dependencies))
                , mainNeedsCoordsArgument(mainNeedsCoordsArgument) {}

        // Mappings used to synthesize function parameters according to dependencies on pipeline
        // input/output variables.
        DepsMap dependencies;

        // True, if the main function takes a coordinate parameter. This is used to ensure that
        // sk_FragCoord is declared as part of pipeline inputs.
        bool mainNeedsCoordsArgument;
    };

    WGSLCodeGenerator(const Context* context, const Program* program, OutputStream* out)
            : INHERITED(context, program, out)
            , fReservedWords({"array",
                              "FSIn",
                              "FSOut",
                              "_globalUniforms",
                              "_GlobalUniforms",
                              "_return",
                              "_stageIn",
                              "_stageOut",
                              "VSIn",
                              "VSOut"}) {}

    bool generateCode() override;

private:
    using INHERITED = CodeGenerator;
    using Precedence = OperatorPrecedence;

    // Called by generateCode() as the first step.
    void preprocessProgram();

    // Write output content while correctly handling indentation.
    void write(std::string_view s);
    void writeLine(std::string_view s = std::string_view());
    void finishLine();
    void writeVariableDecl(const Type& type, std::string_view name, Delimiter delimiter);

    // Helpers to declare a pipeline stage IO parameter declaration.
    void writePipelineIODeclaration(Modifiers modifiers,
                                    const Type& type,
                                    std::string_view name,
                                    Delimiter delimiter);
    void writeUserDefinedIODecl(const Type& type,
                                std::string_view name,
                                int location,
                                Delimiter delimiter);
    void writeBuiltinIODecl(const Type& type,
                            std::string_view name,
                            Builtin builtin,
                            Delimiter delimiter);

    // Write a function definition.
    void writeFunction(const FunctionDefinition& f);
    void writeFunctionDeclaration(const FunctionDeclaration& f);

    // Write the program entry point.
    void writeEntryPoint(const FunctionDefinition& f);

    // Writers for supported statement types.
    void writeStatement(const Statement& s);
    void writeStatements(const StatementArray& statements);
    void writeBlock(const Block& b);
    void writeDoStatement(const DoStatement& expr);
    void writeExpressionStatement(const Expression& expr);
    void writeForStatement(const ForStatement& s);
    void writeIfStatement(const IfStatement& s);
    void writeReturnStatement(const ReturnStatement& s);
    void writeVarDeclaration(const VarDeclaration& varDecl);

    // Synthesizes an LValue for an expression.
    class LValue;
    class PointerLValue;
    class SwizzleLValue;
    class VectorComponentLValue;
    std::unique_ptr<LValue> makeLValue(const Expression& e);

    std::string variableReferenceNameForLValue(const VariableReference& r);

    // Writers for expressions. These return the final expression text as a string, and emit any
    // necessary setup code directly into the program as necessary. The returned expression may be
    // a `let`-alias that cannot be assigned-into; use `makeLValue` for an assignable expression.
    std::string assembleExpression(const Expression& e, Precedence parentPrecedence);
    std::string assembleBinaryExpression(const BinaryExpression& b, Precedence parentPrecedence);
    std::string assembleBinaryExpression(const Expression& left,
                                         Operator op,
                                         const Expression& right,
                                         const Type& resultType,
                                         Precedence parentPrecedence);
    std::string assembleFieldAccess(const FieldAccess& f);
    std::string assembleFunctionCall(const FunctionCall& call, Precedence parentPrecedence);
    std::string assembleIndexExpression(const IndexExpression& i);
    std::string assembleLiteral(const Literal& l);
    std::string assemblePostfixExpression(const PostfixExpression& p, Precedence parentPrecedence);
    std::string assemblePrefixExpression(const PrefixExpression& p, Precedence parentPrecedence);
    std::string assembleSwizzle(const Swizzle& swizzle);
    std::string assembleTernaryExpression(const TernaryExpression& t, Precedence parentPrecedence);
    std::string assembleVariableReference(const VariableReference& r);
    std::string assembleName(std::string_view name);

    // Constructor expressions
    std::string assembleAnyConstructor(const AnyConstructor& c, Precedence parentPrecedence);
    std::string assembleConstructorCompound(const ConstructorCompound& c,
                                            Precedence parentPrecedence);
    std::string assembleConstructorCompoundVector(const ConstructorCompound& c,
                                                  Precedence parentPrecedence);
    std::string assembleConstructorCompoundMatrix(const ConstructorCompound& c,
                                                  Precedence parentPrecedence);
    std::string assembleConstructorDiagonalMatrix(const ConstructorDiagonalMatrix& c,
                                                  Precedence parentPrecedence);
    std::string assembleConstructorMatrixResize(const ConstructorMatrixResize& ctor,
                                                Precedence parentPrecedence);

    // Synthesized helper functions for comparison operators that are not supported by WGSL.
    std::string assembleMatrixEqualityExpression(const Expression& left, const Expression& right);

    // Writes a scratch variable into the program and returns its name (e.g. `_skTemp123`).
    std::string writeScratchVar(const Type& type, const std::string& value = "");

    // Writes a scratch let-variable into the program, gives it the value of `expr`, and returns its
    // name (e.g. `_skTemp123`).
    std::string writeScratchLet(const std::string& expr);

    // Converts `expr` into a string and returns a scratch let-variable associated with the
    // expression. Compile-time constants and plain variable references will return the expression
    // directly and omit the let-variable.
    std::string writeNontrivialScratchLet(const Expression& expr, Precedence parentPrecedence);

    // Generic recursive ProgramElement visitor.
    void writeProgramElement(const ProgramElement& e);
    void writeGlobalVarDeclaration(const GlobalVarDeclaration& d);
    void writeStructDefinition(const StructDefinition& s);

    // Writes the WGSL struct fields for SkSL structs and interface blocks. Enforces WGSL address
    // space layout constraints
    // (https://www.w3.org/TR/WGSL/#address-space-layout-constraints) if a `layout` is
    // provided. A struct that does not need to be host-shareable does not require a `layout`.
    void writeFields(SkSpan<const Field> fields,
                     Position parentPos,
                     const MemoryLayout* layout = nullptr);

    // We bundle all varying pipeline stage inputs and outputs in a struct.
    void writeStageInputStruct();
    void writeStageOutputStruct();

    // Writes all top-level non-opaque global uniform declarations (i.e. not part of an interface
    // block) into a single uniform block binding.
    //
    // In complete fragment/vertex/compute programs, uniforms will be declared only as interface
    // blocks and global opaque types (like textures and samplers) which we expect to be declared
    // with a unique binding and descriptor set index. However, test files that are declared as RTE
    // programs may contain OpenGL-style global uniform declarations with no clear binding index to
    // use for the containing synthesized block.
    //
    // Since we are handling these variables only to generate gold files from RTEs and never run
    // them, we always declare them at the default bind group and binding index.
    void writeNonBlockUniformsForTests();

    // For a given function declaration, writes out any implicitly required pipeline stage arguments
    // based on the function's pre-determined dependencies. These are expected to be written out as
    // the first parameters for a function that requires them. Returns true if any arguments were
    // written.
    std::string functionDependencyArgs(const FunctionDeclaration&);
    bool writeFunctionDependencyParams(const FunctionDeclaration&);

    // Stores the disallowed identifier names.
    skia_private::THashSet<std::string_view> fReservedWords;
    ProgramRequirements fRequirements;
    int fPipelineInputCount = 0;
    bool fDeclaredUniformsStruct = false;

    // Output processing state.
    int fIndentation = 0;
    bool fAtLineStart = false;

    int fScratchCount = 0;
};

}  // namespace SkSL

namespace sknonstd {
template <>
struct is_bitmask_enum<SkSL::WGSLCodeGenerator::FunctionDependencies> : std::true_type {};
}  // namespace sknonstd

#endif  // SKSL_WGSLCODEGENERATOR
