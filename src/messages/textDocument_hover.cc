// Copyright 2017-2018 ccls Authors
// SPDX-License-Identifier: Apache-2.0

#include "message_handler.hh"
#include "query.hh"

#include <string_view>
using namespace std::literals;

namespace ccls {
namespace {
enum MarkupKind {
  Plaintext,
  Markdown,
};
struct MarkupContent {
  std::string kind;
  std::string value;
};
struct MarkedString {
  std::optional<std::string> language;
  std::string value;
};
/// \todo Allow for both MarkedString and MarkupContent based on client capabilities and requests.
struct Hover {
  MarkupContent contents;
  std::optional<lsRange> range;
};

void reflect(JsonWriter &vis, MarkupContent &v) {
  vis.startObject();
  REFLECT_MEMBER(kind);
  REFLECT_MEMBER(value);
  vis.endObject();
}
void reflect(JsonWriter &vis, MarkedString &v) {
  // If there is a language, emit a `{language:string, value:string}` object. If
  // not, emit a string.
  if (v.language) {
    vis.startObject();
    REFLECT_MEMBER(language);
    REFLECT_MEMBER(value);
    vis.endObject();
  } else {
    reflect(vis, v.value);
  }
}
REFLECT_STRUCT(Hover, contents, range);

const char *languageIdentifier(LanguageId lang) {
  switch (lang) {
  // clang-format off
  case LanguageId::C: return "c";
  case LanguageId::Cpp: return "cpp";
  case LanguageId::ObjC: return "objective-c";
  case LanguageId::ObjCpp: return "objective-cpp";
  default: return "";
    // clang-format on
  }
}

std::string buildSignature(std::string_view s) {
  std::string result;
  result.append("```\n"sv);
  result.append(s);
  result.append("\n```"sv);
  return result;
}

// Returns the hover or detailed name for `sym`, if any.
std::optional<MarkupContent>
getHover(DB *db, LanguageId lang, SymbolRef sym, int file_id) {
  const char *comments = nullptr;
  std::optional<MarkupContent> hover;
  withEntity(db, sym, [&](const auto &entity) {
    for (auto &d : entity.def) {
      if (!comments && d.comments[0])
        comments = d.comments;
      if (d.spell) {
        if (d.comments[0])
          comments = d.comments;
        if (const char *s =
                d.hover[0] ? d.hover
                           : d.detailed_name[0] ? d.detailed_name : nullptr) {
          if (!hover)
            hover = {"markdown", buildSignature(s)};
          else if (strlen(s) > hover->value.size())
            hover->value = buildSignature(s);
        }
        if (d.spell->file_id == file_id)
          break;
      }
    }
    if (!hover && entity.def.size()) {
      auto &d = entity.def[0];
      hover = {"markdown"};
      if (d.hover[0])
        hover->value = buildSignature(d.hover);
      else if (d.detailed_name[0])
        hover->value = buildSignature(d.detailed_name);
    }
    if (comments) {
      hover->value.append("\n\n"sv);
      hover->value.append(comments);
    }
  });
  return hover;
}
} // namespace

void MessageHandler::textDocument_hover(TextDocumentPositionParam &param,
                                        ReplyOnce &reply) {
  auto [file, wf] = findOrFail(param.textDocument.uri.getPath(), reply);
  if (!wf)
    return;

  Hover result;
  for (SymbolRef sym : findSymbolsAtLocation(wf, file, param.position)) {
    std::optional<lsRange> ls_range =
        getLsRange(wfiles->getFile(file->def->path), sym.range);
    if (!ls_range)
      continue;

    if (auto hover = getHover(db, file->def->language, sym, file->id); hover) {
      result.range = *ls_range;
      result.contents = *hover;
      break;
    }
  }

  if (result.contents.kind.empty()) {
    result.contents.kind = "plaintext";
  }

  reply(result);
}
} // namespace ccls
