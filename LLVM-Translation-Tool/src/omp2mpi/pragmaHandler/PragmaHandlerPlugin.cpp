// all includes from clang Pragma.cpp file
// TODO remove unused
// cannot figure out which include is needed for diag::warn_pragma_ignored
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/Module.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/LexDiagnostic.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/LiteralSupport.h"
#include "clang/Lex/MacroInfo.h"
#include "clang/Lex/ModuleLoader.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/PTHLexer.h"
#include "clang/Lex/Pragma.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorLexer.h"
#include "clang/Lex/Token.h"
#include "clang/Lex/TokenLexer.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/CrashRecoveryContext.h"
#include "llvm/Support/ErrorHandling.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>
using namespace clang;
using namespace llvm;

#define DEBUG_CLANG_PLUGIN 0

#if DEBUG_CLANG_PLUGIN == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

namespace
{

// pragma handler for #pragma omp2mpi comm
class Omp2MPIPragmaHandler : public PragmaHandler
{
  private:
    bool check_if_statement_is_variable_definition(Preprocessor &PP,
                                                   std::vector<Token> statement)
    {
        // there should be at least the ;
        assert(!statement.empty());
        // TODO we need to make shure, this is a declaration of a variable
        // or possible many variable in one statement
        // TODO give error if this is not a variable declaration statement

        DiagnosticsEngine &D = PP.getDiagnostics();
        unsigned ID = D.getCustomDiagID(DiagnosticsEngine::Warning,
                                        "Currently there is no checking if this is a valid "
                                        "statement for the omp2mpi comm pragma");
        D.Report(statement[0].getLocation(), ID);

        return true;
    }

    // gives the string to annotate for this comm mode or empty string if mode is not valid
    StringRef get_valid_comm_mode(Preprocessor &PP, Token &comm_mode_token)
    {

        // note that fano-prefix-Condition must hold for all annotations used by our Pass
        // at least with the current implemenattion of analyzing the annotation within the pass

        auto *comm_pattern_ident = comm_mode_token.getIdentifierInfo();
        assert(comm_pattern_ident != nullptr);

        if (comm_pattern_ident->getName().equals_lower("default"))
        {
            return "\"OMP2MPI_COMM_DEFAULT\"";
        }

        // currently distributed is same as default anyway
        if (comm_pattern_ident->getName().equals_lower("distributed"))
        {
            return "\"OMP2MPI_COMM_DISTRIBUTED\"";
        }
        if (comm_pattern_ident->getName().equals_lower("distributed_mem") ||
            comm_pattern_ident->getName().equals_lower("distributed_memory") ||
            comm_pattern_ident->getName().equals_lower("distributed_memory_aware"))
        {
            return "\"OMP2MPI_COMM_MEM_AWARE_DISTRIBUTED\"";
        }

        if (comm_pattern_ident->getName().equals_lower("reading"))
        {
            return "\"OMP2MPI_COMM_READING\"";
        }

        if (comm_pattern_ident->getName().equals_lower("master") ||
            comm_pattern_ident->getName().equals_lower("master_based"))
        {
            return "\"OMP2MPI_COMM_MASTER\"";
        }

        DiagnosticsEngine &D = PP.getDiagnostics();
        unsigned ID = D.getCustomDiagID(DiagnosticsEngine::Error,
                                        "Unknown communication mode for omp2mpi comm pragma");
        D.Report(comm_mode_token.getLocation(), ID);

        return "";
    }

    // insert the annotation of the communication scheme
    void insert_annotation(Preprocessor &PP, StringRef comm_mode, SourceLocation loc)
    {
        // insert the annotation of attributes
        // __attribute__((annotate("my annotation")))

        Token t0;
        Token t1;
        Token t2;
        Token t3;
        Token t4;
        Token t5;
        Token t6;
        Token t7;
        Token t8;

        t0.startToken();
        t0.setKind(tok::kw___attribute);
        t0.setLocation(loc);

        t1.startToken();
        t1.setKind(tok::l_paren);
        t1.setLocation(loc);

        t2.startToken();
        t2.setKind(tok::l_paren);
        t2.setLocation(loc);

        t3.startToken();
        t3.setKind(tok::identifier);
        t3.setLocation(loc);
        t3.setIdentifierInfo(PP.getIdentifierInfo("annotate"));

        t4.startToken();
        t4.setKind(tok::l_paren);
        t4.setLocation(loc);

        // The actual annotation:
        std::string the_literal = comm_mode.str();
        assert(the_literal.size() > 0);
        t5.startToken();
        t5.setKind(tok::string_literal);
        t5.setLocation(loc);
        t5.setLiteralData(strdup(the_literal.c_str()));
        t5.setLength(the_literal.size());

        t6.startToken();
        t6.setKind(tok::r_paren);
        t6.setLocation(loc);

        t7.startToken();
        t7.setKind(tok::r_paren);
        t7.setLocation(loc);

        t8.startToken();
        t8.setKind(tok::r_paren);
        t8.setLocation(loc);

        PP.EnterTokenStream({t0, t1, t2, t3, t4, t5, t6, t7, t8},
                            /*DisableMacroExpansion=*/false);
    }

    void handle_this_comm_pragma(Preprocessor &PP, Token &comm_mode_token)
    {
        auto *comm_pattern_ident = comm_mode_token.getIdentifierInfo();
        assert(comm_pattern_ident != nullptr);

        PP.EnableBacktrackAtThisPos(); // we will now see if the next line is valid

        Token next_token;
        PP.Lex(next_token);
        assert(next_token.isAtStartOfLine());

        std::vector<Token> next_statement;

        next_statement.push_back(next_token);

        // parse the complete next statement
        while (next_token.isNot(tok::semi))
        {
            PP.Lex(next_token);
            next_statement.push_back(next_token);
        }

        bool continiue = check_if_statement_is_variable_definition(PP, next_statement);

        PP.Backtrack(); // reset lexer as we do not want to consume the next statement

        auto comm_mode = get_valid_comm_mode(PP, comm_mode_token);
        continiue = continiue && comm_mode != "";
        if (continiue)
        {
            // location where the annotation will be inserted
            SourceLocation loc = next_statement[0].getLocation();
            insert_annotation(PP, comm_mode, loc);
        }
        // else an error was already given
    }

  public:
    Omp2MPIPragmaHandler() : PragmaHandler("comm") {}
    void HandlePragma(Preprocessor &PP, PragmaIntroducerKind Introducer, Token &PragmaTok)
    {
        // Handle the pragma
        Debug(errs() << "Handle a omp2mpi comm Pragma:\n";)

            Token comm_pattern_token;
        PP.LexUnexpandedToken(comm_pattern_token);
        auto *comm_pattern_ident = comm_pattern_token.getIdentifierInfo();
        if (comm_pattern_ident != nullptr)
        {
            Debug(errs() << "Option passed to comm pragma: " << comm_pattern_ident->getName()
                         << "\n";)

                Token next_token;
            PP.LexUnexpandedToken(next_token);
            if (next_token.is(tok::eod))
            {
                handle_this_comm_pragma(PP, comm_pattern_token);
            }
            else
            {
                DiagnosticsEngine &D = PP.getDiagnostics();
                unsigned ID = D.getCustomDiagID(DiagnosticsEngine::Error,
                                                "Malformed omp2mpi comm pragma");
                D.Report(next_token.getLocation(), ID);
            }
        }
        else
        {
            // only a warning as no comm Pattern = default
            DiagnosticsEngine &D = PP.getDiagnostics();
            unsigned ID =
                D.getCustomDiagID(DiagnosticsEngine::Warning,
                                  "No communication Pattern given in omp2mpi comm pargma");
            D.Report(comm_pattern_token.getLocation(), ID);
        }
    }
};

// Define a pragma handler namespace for #pragma omp2mpi
class Omp2MPIPragmaNamespace : public PragmaNamespace
{
  public:
    Omp2MPIPragmaNamespace() : PragmaNamespace("omp2mpi")
    {
        AddPragma(new Omp2MPIPragmaHandler);
        // here one may add other handlers
    }
};

static PragmaHandlerRegistry::Add<Omp2MPIPragmaNamespace>
    Y("omp2mpi", "pragma to guide the omp to mpi translation");

} // namespace
