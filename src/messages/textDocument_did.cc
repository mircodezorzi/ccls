// Copyright 2017-2018 ccls Authors
// SPDX-License-Identifier: Apache-2.0

#include "clang_complete.hh"
#include "include_complete.h"
#include "message_handler.hh"
#include "pipeline.hh"
#include "project.hh"
#include "working_files.h"

namespace ccls {
void MessageHandler::textDocument_didChange(TextDocumentDidChangeParam &param) {
  std::string path = param.textDocument.uri.GetPath();
  working_files->OnChange(param);
  if (g_config->index.onChange)
    pipeline::Index(path, {}, IndexMode::OnChange);
  clang_complete->NotifyView(path);
  if (g_config->diagnostics.onChange >= 0)
    clang_complete->DiagnosticsUpdate(path, g_config->diagnostics.onChange);
}

void MessageHandler::textDocument_didClose(TextDocumentParam &param) {
  std::string path = param.textDocument.uri.GetPath();
  working_files->OnClose(param.textDocument);
  clang_complete->OnClose(path);
}

void MessageHandler::textDocument_didOpen(DidOpenTextDocumentParam &param) {
  std::string path = param.textDocument.uri.GetPath();
  WorkingFile *working_file = working_files->OnOpen(param.textDocument);
  if (std::optional<std::string> cached_file_contents =
          pipeline::LoadIndexedContent(path))
    working_file->SetIndexContent(*cached_file_contents);

  ReplyOnce reply;
  QueryFile *file = FindFile(reply, path);
  if (file) {
    EmitSkippedRanges(working_file, *file);
    EmitSemanticHighlight(db, working_file, *file);
  }
  include_complete->AddFile(working_file->filename);

  // Submit new index request if it is not a header file or there is no
  // pending index request.
  std::pair<LanguageId, bool> lang = lookupExtension(path);
  if ((lang.first != LanguageId::Unknown && !lang.second) ||
      !pipeline::pending_index_requests)
    pipeline::Index(path, {}, IndexMode::Normal);

  clang_complete->NotifyView(path);
}

void MessageHandler::textDocument_didSave(TextDocumentParam &param) {
  const std::string &path = param.textDocument.uri.GetPath();
  pipeline::Index(path, {}, IndexMode::Normal);
  clang_complete->NotifySave(path);
}
} // namespace ccls
