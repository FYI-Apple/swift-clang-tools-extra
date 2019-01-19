//===--- Protocol.h - Language Server Protocol Implementation ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains structs based on the LSP specification at
// https://github.com/Microsoft/language-server-protocol/blob/master/protocol.md
//
// This is not meant to be a complete implementation, new interfaces are added
// when they're needed.
//
// Each struct has a toJSON and fromJSON function, that converts between
// the struct and a JSON representation. (See JSON.h)
//
// Some structs also have operator<< serialization. This is for debugging and
// tests, and is not generally machine-readable.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_PROTOCOL_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_PROTOCOL_H

#include "URI.h"
#include "index/SymbolID.h"
#include "llvm/ADT/Optional.h"
#include "llvm/Support/JSON.h"
#include <bitset>
#include <string>
#include <vector>

namespace clang {
namespace clangd {

enum class ErrorCode {
  // Defined by JSON RPC.
  ParseError = -32700,
  InvalidRequest = -32600,
  MethodNotFound = -32601,
  InvalidParams = -32602,
  InternalError = -32603,

  ServerNotInitialized = -32002,
  UnknownErrorCode = -32001,

  // Defined by the protocol.
  RequestCancelled = -32800,
};
// Models an LSP error as an llvm::Error.
class LSPError : public llvm::ErrorInfo<LSPError> {
public:
  std::string Message;
  ErrorCode Code;
  static char ID;

  LSPError(std::string Message, ErrorCode Code)
      : Message(std::move(Message)), Code(Code) {}

  void log(llvm::raw_ostream &OS) const override {
    OS << int(Code) << ": " << Message;
  }
  std::error_code convertToErrorCode() const override {
    return llvm::inconvertibleErrorCode();
  }
};

// URI in "file" scheme for a file.
struct URIForFile {
  URIForFile() = default;

  /// Canonicalizes \p AbsPath via URI.
  ///
  /// File paths in URIForFile can come from index or local AST. Path from
  /// index goes through URI transformation, and the final path is resolved by
  /// URI scheme and could potentially be different from the original path.
  /// Hence, we do the same transformation for all paths.
  ///
  /// Files can be referred to by several paths (e.g. in the presence of links).
  /// Which one we prefer may depend on where we're coming from. \p TUPath is a
  /// hint, and should usually be the main entrypoint file we're processing.
  static URIForFile canonicalize(llvm::StringRef AbsPath,
                                 llvm::StringRef TUPath);

  static llvm::Expected<URIForFile> fromURI(const URI &U,
                                            llvm::StringRef HintPath);

  /// Retrieves absolute path to the file.
  llvm::StringRef file() const { return File; }

  explicit operator bool() const { return !File.empty(); }
  std::string uri() const { return URI::createFile(File).toString(); }

  friend bool operator==(const URIForFile &LHS, const URIForFile &RHS) {
    return LHS.File == RHS.File;
  }

  friend bool operator!=(const URIForFile &LHS, const URIForFile &RHS) {
    return !(LHS == RHS);
  }

  friend bool operator<(const URIForFile &LHS, const URIForFile &RHS) {
    return LHS.File < RHS.File;
  }

private:
  explicit URIForFile(std::string &&File) : File(std::move(File)) {}

  std::string File;
};

/// Serialize/deserialize \p URIForFile to/from a string URI.
llvm::json::Value toJSON(const URIForFile &U);
bool fromJSON(const llvm::json::Value &, URIForFile &);

struct TextDocumentIdentifier {
  /// The text document's URI.
  URIForFile uri;
};
llvm::json::Value toJSON(const TextDocumentIdentifier &);
bool fromJSON(const llvm::json::Value &, TextDocumentIdentifier &);

struct Position {
  /// Line position in a document (zero-based).
  int line = 0;

  /// Character offset on a line in a document (zero-based).
  /// WARNING: this is in UTF-16 codepoints, not bytes or characters!
  /// Use the functions in SourceCode.h to construct/interpret Positions.
  int character = 0;

  friend bool operator==(const Position &LHS, const Position &RHS) {
    return std::tie(LHS.line, LHS.character) ==
           std::tie(RHS.line, RHS.character);
  }
  friend bool operator!=(const Position &LHS, const Position &RHS) {
    return !(LHS == RHS);
  }
  friend bool operator<(const Position &LHS, const Position &RHS) {
    return std::tie(LHS.line, LHS.character) <
           std::tie(RHS.line, RHS.character);
  }
  friend bool operator<=(const Position &LHS, const Position &RHS) {
    return std::tie(LHS.line, LHS.character) <=
           std::tie(RHS.line, RHS.character);
  }
};
bool fromJSON(const llvm::json::Value &, Position &);
llvm::json::Value toJSON(const Position &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const Position &);

struct Range {
  /// The range's start position.
  Position start;

  /// The range's end position.
  Position end;

  friend bool operator==(const Range &LHS, const Range &RHS) {
    return std::tie(LHS.start, LHS.end) == std::tie(RHS.start, RHS.end);
  }
  friend bool operator!=(const Range &LHS, const Range &RHS) {
    return !(LHS == RHS);
  }
  friend bool operator<(const Range &LHS, const Range &RHS) {
    return std::tie(LHS.start, LHS.end) < std::tie(RHS.start, RHS.end);
  }

  bool contains(Position Pos) const { return start <= Pos && Pos < end; }
  bool contains(Range Rng) const {
    return start <= Rng.start && Rng.end <= end;
  }
};
bool fromJSON(const llvm::json::Value &, Range &);
llvm::json::Value toJSON(const Range &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const Range &);

struct Location {
  /// The text document's URI.
  URIForFile uri;
  Range range;

  friend bool operator==(const Location &LHS, const Location &RHS) {
    return LHS.uri == RHS.uri && LHS.range == RHS.range;
  }

  friend bool operator!=(const Location &LHS, const Location &RHS) {
    return !(LHS == RHS);
  }

  friend bool operator<(const Location &LHS, const Location &RHS) {
    return std::tie(LHS.uri, LHS.range) < std::tie(RHS.uri, RHS.range);
  }
};
llvm::json::Value toJSON(const Location &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const Location &);

struct TextEdit {
  /// The range of the text document to be manipulated. To insert
  /// text into a document create a range where start === end.
  Range range;

  /// The string to be inserted. For delete operations use an
  /// empty string.
  std::string newText;

  bool operator==(const TextEdit &rhs) const {
    return newText == rhs.newText && range == rhs.range;
  }
};
bool fromJSON(const llvm::json::Value &, TextEdit &);
llvm::json::Value toJSON(const TextEdit &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const TextEdit &);

struct TextDocumentItem {
  /// The text document's URI.
  URIForFile uri;

  /// The text document's language identifier.
  std::string languageId;

  /// The version number of this document (it will strictly increase after each
  int version = 0;

  /// The content of the opened text document.
  std::string text;
};
bool fromJSON(const llvm::json::Value &, TextDocumentItem &);

enum class TraceLevel {
  Off = 0,
  Messages = 1,
  Verbose = 2,
};
bool fromJSON(const llvm::json::Value &E, TraceLevel &Out);

struct NoParams {};
inline bool fromJSON(const llvm::json::Value &, NoParams &) { return true; }
using ShutdownParams = NoParams;
using ExitParams = NoParams;

/// Defines how the host (editor) should sync document changes to the language
/// server.
enum class TextDocumentSyncKind {
  /// Documents should not be synced at all.
  None = 0,

  /// Documents are synced by always sending the full content of the document.
  Full = 1,

  /// Documents are synced by sending the full content on open.  After that
  /// only incremental updates to the document are send.
  Incremental = 2,
};

/// The kind of a completion entry.
enum class CompletionItemKind {
  Missing = 0,
  Text = 1,
  Method = 2,
  Function = 3,
  Constructor = 4,
  Field = 5,
  Variable = 6,
  Class = 7,
  Interface = 8,
  Module = 9,
  Property = 10,
  Unit = 11,
  Value = 12,
  Enum = 13,
  Keyword = 14,
  Snippet = 15,
  Color = 16,
  File = 17,
  Reference = 18,
  Folder = 19,
  EnumMember = 20,
  Constant = 21,
  Struct = 22,
  Event = 23,
  Operator = 24,
  TypeParameter = 25,
};
bool fromJSON(const llvm::json::Value &, CompletionItemKind &);
constexpr auto CompletionItemKindMin =
    static_cast<size_t>(CompletionItemKind::Text);
constexpr auto CompletionItemKindMax =
    static_cast<size_t>(CompletionItemKind::TypeParameter);
using CompletionItemKindBitset = std::bitset<CompletionItemKindMax + 1>;
bool fromJSON(const llvm::json::Value &, CompletionItemKindBitset &);
CompletionItemKind
adjustKindToCapability(CompletionItemKind Kind,
                       CompletionItemKindBitset &supportedCompletionItemKinds);

/// A symbol kind.
enum class SymbolKind {
  File = 1,
  Module = 2,
  Namespace = 3,
  Package = 4,
  Class = 5,
  Method = 6,
  Property = 7,
  Field = 8,
  Constructor = 9,
  Enum = 10,
  Interface = 11,
  Function = 12,
  Variable = 13,
  Constant = 14,
  String = 15,
  Number = 16,
  Boolean = 17,
  Array = 18,
  Object = 19,
  Key = 20,
  Null = 21,
  EnumMember = 22,
  Struct = 23,
  Event = 24,
  Operator = 25,
  TypeParameter = 26
};
bool fromJSON(const llvm::json::Value &, SymbolKind &);
constexpr auto SymbolKindMin = static_cast<size_t>(SymbolKind::File);
constexpr auto SymbolKindMax = static_cast<size_t>(SymbolKind::TypeParameter);
using SymbolKindBitset = std::bitset<SymbolKindMax + 1>;
bool fromJSON(const llvm::json::Value &, SymbolKindBitset &);
SymbolKind adjustKindToCapability(SymbolKind Kind,
                                  SymbolKindBitset &supportedSymbolKinds);

// This struct doesn't mirror LSP!
// The protocol defines deeply nested structures for client capabilities.
// Instead of mapping them all, this just parses out the bits we care about.
struct ClientCapabilities {
  /// The supported set of SymbolKinds for workspace/symbol.
  /// workspace.symbol.symbolKind.valueSet
  llvm::Optional<SymbolKindBitset> WorkspaceSymbolKinds;

  /// Whether the client accepts diagnostics with codeActions attached inline.
  /// textDocument.publishDiagnostics.codeActionsInline.
  bool DiagnosticFixes = false;

  /// Whether the client accepts diagnostics with category attached to it
  /// using the "category" extension.
  /// textDocument.publishDiagnostics.categorySupport
  bool DiagnosticCategory = false;

  /// Client supports snippets as insert text.
  /// textDocument.completion.completionItem.snippetSupport
  bool CompletionSnippets = false;

  /// Client supports hierarchical document symbols.
  bool HierarchicalDocumentSymbol = false;

  /// The supported set of CompletionItemKinds for textDocument/completion.
  /// textDocument.completion.completionItemKind.valueSet
  llvm::Optional<CompletionItemKindBitset> CompletionItemKinds;

  /// Client supports CodeAction return value for textDocument/codeAction.
  /// textDocument.codeAction.codeActionLiteralSupport.
  bool CodeActionStructure = false;
};
bool fromJSON(const llvm::json::Value &, ClientCapabilities &);

/// Clangd extension that's used in the 'compilationDatabaseChanges' in
/// workspace/didChangeConfiguration to record updates to the in-memory
/// compilation database.
struct ClangdCompileCommand {
  std::string workingDirectory;
  std::vector<std::string> compilationCommand;
};
bool fromJSON(const llvm::json::Value &, ClangdCompileCommand &);

/// Clangd extension: parameters configurable at any time, via the
/// `workspace/didChangeConfiguration` notification.
/// LSP defines this type as `any`.
struct ConfigurationSettings {
  // Changes to the in-memory compilation database.
  // The key of the map is a file name.
  std::map<std::string, ClangdCompileCommand> compilationDatabaseChanges;
};
bool fromJSON(const llvm::json::Value &, ConfigurationSettings &);

/// Clangd extension: parameters configurable at `initialize` time.
/// LSP defines this type as `any`.
struct InitializationOptions {
  // What we can change throught the didChangeConfiguration request, we can
  // also set through the initialize request (initializationOptions field).
  ConfigurationSettings ConfigSettings;

  llvm::Optional<std::string> compilationDatabasePath;
  // Additional flags to be included in the "fallback command" used when
  // the compilation database doesn't describe an opened file.
  // The command used will be approximately `clang $FILE $fallbackFlags`.
  std::vector<std::string> fallbackFlags;

  /// Clients supports show file status for textDocument/clangd.fileStatus.
  bool FileStatus = false;
};
bool fromJSON(const llvm::json::Value &, InitializationOptions &);

struct InitializeParams {
  /// The process Id of the parent process that started
  /// the server. Is null if the process has not been started by another
  /// process. If the parent process is not alive then the server should exit
  /// (see exit notification) its process.
  llvm::Optional<int> processId;

  /// The rootPath of the workspace. Is null
  /// if no folder is open.
  ///
  /// @deprecated in favour of rootUri.
  llvm::Optional<std::string> rootPath;

  /// The rootUri of the workspace. Is null if no
  /// folder is open. If both `rootPath` and `rootUri` are set
  /// `rootUri` wins.
  llvm::Optional<URIForFile> rootUri;

  // User provided initialization options.
  // initializationOptions?: any;

  /// The capabilities provided by the client (editor or tool)
  ClientCapabilities capabilities;

  /// The initial trace setting. If omitted trace is disabled ('off').
  llvm::Optional<TraceLevel> trace;

  /// User-provided initialization options.
  InitializationOptions initializationOptions;
};
bool fromJSON(const llvm::json::Value &, InitializeParams &);

struct DidOpenTextDocumentParams {
  /// The document that was opened.
  TextDocumentItem textDocument;
};
bool fromJSON(const llvm::json::Value &, DidOpenTextDocumentParams &);

struct DidCloseTextDocumentParams {
  /// The document that was closed.
  TextDocumentIdentifier textDocument;
};
bool fromJSON(const llvm::json::Value &, DidCloseTextDocumentParams &);

struct TextDocumentContentChangeEvent {
  /// The range of the document that changed.
  llvm::Optional<Range> range;

  /// The length of the range that got replaced.
  llvm::Optional<int> rangeLength;

  /// The new text of the range/document.
  std::string text;
};
bool fromJSON(const llvm::json::Value &, TextDocumentContentChangeEvent &);

struct DidChangeTextDocumentParams {
  /// The document that did change. The version number points
  /// to the version after all provided content changes have
  /// been applied.
  TextDocumentIdentifier textDocument;

  /// The actual content changes.
  std::vector<TextDocumentContentChangeEvent> contentChanges;

  /// Forces diagnostics to be generated, or to not be generated, for this
  /// version of the file. If not set, diagnostics are eventually consistent:
  /// either they will be provided for this version or some subsequent one.
  /// This is a clangd extension.
  llvm::Optional<bool> wantDiagnostics;
};
bool fromJSON(const llvm::json::Value &, DidChangeTextDocumentParams &);

enum class FileChangeType {
  /// The file got created.
  Created = 1,
  /// The file got changed.
  Changed = 2,
  /// The file got deleted.
  Deleted = 3
};
bool fromJSON(const llvm::json::Value &E, FileChangeType &Out);

struct FileEvent {
  /// The file's URI.
  URIForFile uri;
  /// The change type.
  FileChangeType type = FileChangeType::Created;
};
bool fromJSON(const llvm::json::Value &, FileEvent &);

struct DidChangeWatchedFilesParams {
  /// The actual file events.
  std::vector<FileEvent> changes;
};
bool fromJSON(const llvm::json::Value &, DidChangeWatchedFilesParams &);

struct DidChangeConfigurationParams {
  ConfigurationSettings settings;
};
bool fromJSON(const llvm::json::Value &, DidChangeConfigurationParams &);

struct FormattingOptions {
  /// Size of a tab in spaces.
  int tabSize = 0;

  /// Prefer spaces over tabs.
  bool insertSpaces = false;
};
bool fromJSON(const llvm::json::Value &, FormattingOptions &);
llvm::json::Value toJSON(const FormattingOptions &);

struct DocumentRangeFormattingParams {
  /// The document to format.
  TextDocumentIdentifier textDocument;

  /// The range to format
  Range range;

  /// The format options
  FormattingOptions options;
};
bool fromJSON(const llvm::json::Value &, DocumentRangeFormattingParams &);

struct DocumentOnTypeFormattingParams {
  /// The document to format.
  TextDocumentIdentifier textDocument;

  /// The position at which this request was sent.
  Position position;

  /// The character that has been typed.
  std::string ch;

  /// The format options.
  FormattingOptions options;
};
bool fromJSON(const llvm::json::Value &, DocumentOnTypeFormattingParams &);

struct DocumentFormattingParams {
  /// The document to format.
  TextDocumentIdentifier textDocument;

  /// The format options
  FormattingOptions options;
};
bool fromJSON(const llvm::json::Value &, DocumentFormattingParams &);

struct DocumentSymbolParams {
  // The text document to find symbols in.
  TextDocumentIdentifier textDocument;
};
bool fromJSON(const llvm::json::Value &, DocumentSymbolParams &);

struct CodeAction;
struct Diagnostic {
  /// The range at which the message applies.
  Range range;

  /// The diagnostic's severity. Can be omitted. If omitted it is up to the
  /// client to interpret diagnostics as error, warning, info or hint.
  int severity = 0;

  /// The diagnostic's code. Can be omitted.
  /// Note: Not currently used by clangd
  // std::string code;

  /// A human-readable string describing the source of this
  /// diagnostic, e.g. 'typescript' or 'super lint'.
  /// Note: Not currently used by clangd
  // std::string source;

  /// The diagnostic's message.
  std::string message;

  /// The diagnostic's category. Can be omitted.
  /// An LSP extension that's used to send the name of the category over to the
  /// client. The category typically describes the compilation stage during
  /// which the issue was produced, e.g. "Semantic Issue" or "Parse Issue".
  llvm::Optional<std::string> category;

  /// Clangd extension: code actions related to this diagnostic.
  /// Only with capability textDocument.publishDiagnostics.codeActionsInline.
  /// (These actions can also be obtained using textDocument/codeAction).
  llvm::Optional<std::vector<CodeAction>> codeActions;
};
llvm::json::Value toJSON(const Diagnostic &);

/// A LSP-specific comparator used to find diagnostic in a container like
/// std:map.
/// We only use the required fields of Diagnostic to do the comparsion to avoid
/// any regression issues from LSP clients (e.g. VScode), see
/// https://git.io/vbr29
struct LSPDiagnosticCompare {
  bool operator()(const Diagnostic &LHS, const Diagnostic &RHS) const {
    return std::tie(LHS.range, LHS.message) < std::tie(RHS.range, RHS.message);
  }
};
bool fromJSON(const llvm::json::Value &, Diagnostic &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const Diagnostic &);

struct CodeActionContext {
  /// An array of diagnostics.
  std::vector<Diagnostic> diagnostics;
};
bool fromJSON(const llvm::json::Value &, CodeActionContext &);

struct CodeActionParams {
  /// The document in which the command was invoked.
  TextDocumentIdentifier textDocument;

  /// The range for which the command was invoked.
  Range range;

  /// Context carrying additional information.
  CodeActionContext context;
};
bool fromJSON(const llvm::json::Value &, CodeActionParams &);

struct WorkspaceEdit {
  /// Holds changes to existing resources.
  llvm::Optional<std::map<std::string, std::vector<TextEdit>>> changes;

  /// Note: "documentChanges" is not currently used because currently there is
  /// no support for versioned edits.
};
bool fromJSON(const llvm::json::Value &, WorkspaceEdit &);
llvm::json::Value toJSON(const WorkspaceEdit &WE);

/// Exact commands are not specified in the protocol so we define the
/// ones supported by Clangd here. The protocol specifies the command arguments
/// to be "any[]" but to make this safer and more manageable, each command we
/// handle maps to a certain llvm::Optional of some struct to contain its
/// arguments. Different commands could reuse the same llvm::Optional as
/// arguments but a command that needs different arguments would simply add a
/// new llvm::Optional and not use any other ones. In practice this means only
/// one argument type will be parsed and set.
struct ExecuteCommandParams {
  // Command to apply fix-its. Uses WorkspaceEdit as argument.
  const static llvm::StringLiteral CLANGD_APPLY_FIX_COMMAND;

  /// The command identifier, e.g. CLANGD_APPLY_FIX_COMMAND
  std::string command;

  // Arguments
  llvm::Optional<WorkspaceEdit> workspaceEdit;
};
bool fromJSON(const llvm::json::Value &, ExecuteCommandParams &);

struct Command : public ExecuteCommandParams {
  std::string title;
};
llvm::json::Value toJSON(const Command &C);

/// A code action represents a change that can be performed in code, e.g. to fix
/// a problem or to refactor code.
///
/// A CodeAction must set either `edit` and/or a `command`. If both are
/// supplied, the `edit` is applied first, then the `command` is executed.
struct CodeAction {
  /// A short, human-readable, title for this code action.
  std::string title;

  /// The kind of the code action.
  /// Used to filter code actions.
  llvm::Optional<std::string> kind;
  const static llvm::StringLiteral QUICKFIX_KIND;

  /// The diagnostics that this code action resolves.
  llvm::Optional<std::vector<Diagnostic>> diagnostics;

  /// The workspace edit this code action performs.
  llvm::Optional<WorkspaceEdit> edit;

  /// A command this code action executes. If a code action provides an edit
  /// and a command, first the edit is executed and then the command.
  llvm::Optional<Command> command;
};
llvm::json::Value toJSON(const CodeAction &);

/// Represents programming constructs like variables, classes, interfaces etc.
/// that appear in a document. Document symbols can be hierarchical and they
/// have two ranges: one that encloses its definition and one that points to its
/// most interesting range, e.g. the range of an identifier.
struct DocumentSymbol {
  /// The name of this symbol.
  std::string name;

  /// More detail for this symbol, e.g the signature of a function.
  std::string detail;

  /// The kind of this symbol.
  SymbolKind kind;

  /// Indicates if this symbol is deprecated.
  bool deprecated;

  /// The range enclosing this symbol not including leading/trailing whitespace
  /// but everything else like comments. This information is typically used to
  /// determine if the clients cursor is inside the symbol to reveal in the
  /// symbol in the UI.
  Range range;

  /// The range that should be selected and revealed when this symbol is being
  /// picked, e.g the name of a function. Must be contained by the `range`.
  Range selectionRange;

  /// Children of this symbol, e.g. properties of a class.
  std::vector<DocumentSymbol> children;
};
llvm::raw_ostream &operator<<(llvm::raw_ostream &O, const DocumentSymbol &S);
llvm::json::Value toJSON(const DocumentSymbol &S);

/// Represents information about programming constructs like variables, classes,
/// interfaces etc.
struct SymbolInformation {
  /// The name of this symbol.
  std::string name;

  /// The kind of this symbol.
  SymbolKind kind;

  /// The location of this symbol.
  Location location;

  /// The name of the symbol containing this symbol.
  std::string containerName;
};
llvm::json::Value toJSON(const SymbolInformation &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const SymbolInformation &);

/// Represents information about identifier.
/// This is returned from textDocument/symbolInfo, which is a clangd extension.
struct SymbolDetails {
  std::string name;

  std::string containerName;

  /// Unified Symbol Resolution identifier
  /// This is an opaque string uniquely identifying a symbol.
  /// Unlike SymbolID, it is variable-length and somewhat human-readable.
  /// It is a common representation across several clang tools.
  /// (See USRGeneration.h)
  std::string USR;

  llvm::Optional<SymbolID> ID;
};
llvm::json::Value toJSON(const SymbolDetails &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const SymbolDetails &);
bool operator==(const SymbolDetails &, const SymbolDetails &);

/// The parameters of a Workspace Symbol Request.
struct WorkspaceSymbolParams {
  /// A non-empty query string
  std::string query;
};
bool fromJSON(const llvm::json::Value &, WorkspaceSymbolParams &);

struct ApplyWorkspaceEditParams {
  WorkspaceEdit edit;
};
llvm::json::Value toJSON(const ApplyWorkspaceEditParams &);

struct TextDocumentPositionParams {
  /// The text document.
  TextDocumentIdentifier textDocument;

  /// The position inside the text document.
  Position position;
};
bool fromJSON(const llvm::json::Value &, TextDocumentPositionParams &);

enum class CompletionTriggerKind {
  /// Completion was triggered by typing an identifier (24x7 code
  /// complete), manual invocation (e.g Ctrl+Space) or via API.
  Invoked = 1,
  /// Completion was triggered by a trigger character specified by
  /// the `triggerCharacters` properties of the `CompletionRegistrationOptions`.
  TriggerCharacter = 2,
  /// Completion was re-triggered as the current completion list is incomplete.
  TriggerTriggerForIncompleteCompletions = 3
};

struct CompletionContext {
  /// How the completion was triggered.
  CompletionTriggerKind triggerKind = CompletionTriggerKind::Invoked;
  /// The trigger character (a single character) that has trigger code complete.
  /// Is undefined if `triggerKind !== CompletionTriggerKind.TriggerCharacter`
  std::string triggerCharacter;
};
bool fromJSON(const llvm::json::Value &, CompletionContext &);

struct CompletionParams : TextDocumentPositionParams {
  CompletionContext context;
};
bool fromJSON(const llvm::json::Value &, CompletionParams &);

enum class MarkupKind {
  PlainText,
  Markdown,
};

struct MarkupContent {
  MarkupKind kind = MarkupKind::PlainText;
  std::string value;
};
llvm::json::Value toJSON(const MarkupContent &MC);

struct Hover {
  /// The hover's content
  MarkupContent contents;

  /// An optional range is a range inside a text document
  /// that is used to visualize a hover, e.g. by changing the background color.
  llvm::Optional<Range> range;
};
llvm::json::Value toJSON(const Hover &H);

/// Defines whether the insert text in a completion item should be interpreted
/// as plain text or a snippet.
enum class InsertTextFormat {
  Missing = 0,
  /// The primary text to be inserted is treated as a plain string.
  PlainText = 1,
  /// The primary text to be inserted is treated as a snippet.
  ///
  /// A snippet can define tab stops and placeholders with `$1`, `$2`
  /// and `${3:foo}`. `$0` defines the final tab stop, it defaults to the end
  /// of the snippet. Placeholders with equal identifiers are linked, that is
  /// typing in one will update others too.
  ///
  /// See also:
  /// https//github.com/Microsoft/vscode/blob/master/src/vs/editor/contrib/snippet/common/snippet.md
  Snippet = 2,
};

struct CompletionItem {
  /// The label of this completion item. By default also the text that is
  /// inserted when selecting this completion.
  std::string label;

  /// The kind of this completion item. Based of the kind an icon is chosen by
  /// the editor.
  CompletionItemKind kind = CompletionItemKind::Missing;

  /// A human-readable string with additional information about this item, like
  /// type or symbol information.
  std::string detail;

  /// A human-readable string that represents a doc-comment.
  std::string documentation;

  /// A string that should be used when comparing this item with other items.
  /// When `falsy` the label is used.
  std::string sortText;

  /// A string that should be used when filtering a set of completion items.
  /// When `falsy` the label is used.
  std::string filterText;

  /// A string that should be inserted to a document when selecting this
  /// completion. When `falsy` the label is used.
  std::string insertText;

  /// The format of the insert text. The format applies to both the `insertText`
  /// property and the `newText` property of a provided `textEdit`.
  InsertTextFormat insertTextFormat = InsertTextFormat::Missing;

  /// An edit which is applied to a document when selecting this completion.
  /// When an edit is provided `insertText` is ignored.
  ///
  /// Note: The range of the edit must be a single line range and it must
  /// contain the position at which completion has been requested.
  llvm::Optional<TextEdit> textEdit;

  /// An optional array of additional text edits that are applied when selecting
  /// this completion. Edits must not overlap with the main edit nor with
  /// themselves.
  std::vector<TextEdit> additionalTextEdits;

  /// Indicates if this item is deprecated.
  bool deprecated = false;

  // TODO(krasimir): The following optional fields defined by the language
  // server protocol are unsupported:
  //
  // data?: any - A data entry field that is preserved on a completion item
  //              between a completion and a completion resolve request.
};
llvm::json::Value toJSON(const CompletionItem &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const CompletionItem &);

bool operator<(const CompletionItem &, const CompletionItem &);

/// Represents a collection of completion items to be presented in the editor.
struct CompletionList {
  /// The list is not complete. Further typing should result in recomputing the
  /// list.
  bool isIncomplete = false;

  /// The completion items.
  std::vector<CompletionItem> items;
};
llvm::json::Value toJSON(const CompletionList &);

/// A single parameter of a particular signature.
struct ParameterInformation {

  /// The label of this parameter. Mandatory.
  std::string label;

  /// The documentation of this parameter. Optional.
  std::string documentation;
};
llvm::json::Value toJSON(const ParameterInformation &);

/// Represents the signature of something callable.
struct SignatureInformation {

  /// The label of this signature. Mandatory.
  std::string label;

  /// The documentation of this signature. Optional.
  std::string documentation;

  /// The parameters of this signature.
  std::vector<ParameterInformation> parameters;
};
llvm::json::Value toJSON(const SignatureInformation &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &,
                              const SignatureInformation &);

/// Represents the signature of a callable.
struct SignatureHelp {

  /// The resulting signatures.
  std::vector<SignatureInformation> signatures;

  /// The active signature.
  int activeSignature = 0;

  /// The active parameter of the active signature.
  int activeParameter = 0;

  /// Position of the start of the argument list, including opening paren. e.g.
  /// foo("first arg",   "second arg",
  ///    ^-argListStart   ^-cursor
  /// This is a clangd-specific extension, it is only available via C++ API and
  /// not currently serialized for the LSP.
  Position argListStart;
};
llvm::json::Value toJSON(const SignatureHelp &);

struct RenameParams {
  /// The document that was opened.
  TextDocumentIdentifier textDocument;

  /// The position at which this request was sent.
  Position position;

  /// The new name of the symbol.
  std::string newName;
};
bool fromJSON(const llvm::json::Value &, RenameParams &);

enum class DocumentHighlightKind { Text = 1, Read = 2, Write = 3 };

/// A document highlight is a range inside a text document which deserves
/// special attention. Usually a document highlight is visualized by changing
/// the background color of its range.

struct DocumentHighlight {
  /// The range this highlight applies to.
  Range range;

  /// The highlight kind, default is DocumentHighlightKind.Text.
  DocumentHighlightKind kind = DocumentHighlightKind::Text;

  friend bool operator<(const DocumentHighlight &LHS,
                        const DocumentHighlight &RHS) {
    int LHSKind = static_cast<int>(LHS.kind);
    int RHSKind = static_cast<int>(RHS.kind);
    return std::tie(LHS.range, LHSKind) < std::tie(RHS.range, RHSKind);
  }

  friend bool operator==(const DocumentHighlight &LHS,
                         const DocumentHighlight &RHS) {
    return LHS.kind == RHS.kind && LHS.range == RHS.range;
  }
};
llvm::json::Value toJSON(const DocumentHighlight &DH);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const DocumentHighlight &);

struct ReferenceParams : public TextDocumentPositionParams {
  // For now, no options like context.includeDeclaration are supported.
};
bool fromJSON(const llvm::json::Value &, ReferenceParams &);

/// Clangd extension: indicates the current state of the file in clangd,
/// sent from server via the `textDocument/clangd.fileStatus` notification.
struct FileStatus {
  /// The text document's URI.
  URIForFile uri;
  /// The human-readable string presents the current state of the file, can be
  /// shown in the UI (e.g. status bar).
  std::string state;
  // FIXME: add detail messages.
};
llvm::json::Value toJSON(const FileStatus &FStatus);

} // namespace clangd
} // namespace clang

namespace llvm {
template <> struct format_provider<clang::clangd::Position> {
  static void format(const clang::clangd::Position &Pos, raw_ostream &OS,
                     StringRef Style) {
    assert(Style.empty() && "style modifiers for this type are not supported");
    OS << Pos;
  }
};
} // namespace llvm

#endif
