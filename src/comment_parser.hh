#pragma once

#include "log.hh"
#include "serializer.hh"

#include <clang/AST/AST.h>
#include <clang/AST/Comment.h>
#include <clang/AST/CommentCommandTraits.h>
#include <clang/AST/CommentLexer.h>

#include <string_view>
using namespace std::literals;

using namespace clang;

namespace ccls {

/// \todo Optimizations.
/// \todo Check client capabilities option.
class CommentParser {
public:
  ASTContext *ctx;

  llvm::BumpPtrAllocator allocator;
  SourceManager &sm;
  DiagnosticsEngine &diag;
  comments::CommandTraits &traits;

  CommentParser(ASTContext *ctx)
      : ctx{ctx}, allocator{ctx->getAllocator()}, sm{ctx->getSourceManager()},
        diag{ctx->getDiagnostics()}, traits{ctx->getCommentCommandTraits()} {}

  /// \todo This should probably be moved inside of the source file.
  std::string Parse(const RawComment *rc) {
    StringRef raw = rc->getRawText(ctx->getSourceManager());

    comments::Lexer L(allocator, diag, traits, rc->getBeginLoc(), raw.data(), raw.end());
    comments::Token Tok;

    std::string brief = "";
    std::string returns = "";
    std::string params = "";

    std::string result = "";

    L.lex(Tok);
    while (Tok.isNot(comments::tok::eof)) {
      if (Tok.is(comments::tok::backslash_command) ||
          Tok.is(comments::tok::at_command)) {
        const comments::CommandInfo *Info = traits.getCommandInfo(Tok.getCommandID());
        switch (Info->getID()) {
        case comments::CommandTraits::KnownCommandIDs::KCI_brief:
          L.lex(Tok);
          if (Tok.getKind() == comments::tok::text) {
            brief = Tok.getText();
          }
          break;
        case comments::CommandTraits::KnownCommandIDs::KCI_param:
          L.lex(Tok);
          if (Tok.getKind() == comments::tok::text) {
            StringRef s = Tok.getText();
            size_t pos = s.find(' ', 1);
            params.append("- `"sv);
            params.append(s.substr(1, pos - 1));
            params.append("`: "sv);
            params.append(s.substr(pos + 1, s.size() - 1));
            params.append("\n"sv);
          }
          break;
        }
      }
      L.lex(Tok);
    }

    if (!brief.empty()) {
      result.append("*Brief*: "sv);
      result.append(brief);
      result.append("\n"sv);
    }
    if (!params.empty()) {
      result.append("# Parameters\n"sv);
      result.append(params);
      result.append("\n"sv);
    }
    
    return result;
  }
};

}; // namespace ccls