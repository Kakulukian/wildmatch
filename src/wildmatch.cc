#include <napi.h>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>

namespace wildmatch
{

  enum Flags
  {
    WM_CASEFOLD = 1,
    WM_PATHNAME = 2
  };

  enum Result
  {
    WM_MATCH = 0,
    WM_NOMATCH = 1,
    WM_ABORT_ALL = -1,
    WM_ABORT_TO_STARSTAR = -2
  };

#define NEGATE_CLASS '!'
#define NEGATE_CLASS2 '^'

#define ISASCII(c) 1
#define ISUPPER(c) (ISASCII(c) && std::isupper(c))
#define ISLOWER(c) (ISASCII(c) && std::islower(c))
#define ISALPHA(c) (ISASCII(c) && std::isalpha(c))
#define ISDIGIT(c) (ISASCII(c) && std::isdigit(c))
#define ISALNUM(c) (ISASCII(c) && std::isalnum(c))
#define ISGRAPH(c) (ISASCII(c) && std::isgraph(c))
#define ISPRINT(c) (ISASCII(c) && std::isprint(c))
#define ISCNTRL(c) (ISASCII(c) && std::iscntrl(c))
#define ISPUNCT(c) (ISASCII(c) && std::ispunct(c))
#define ISSPACE(c) (ISASCII(c) && std::isspace(c))
#define ISXDIGIT(c) (ISASCII(c) && std::isxdigit(c))
#define ISBLANK(c) ((c) == ' ' || (c) == '\t')

  inline bool is_glob_special(char c)
  {
    return c == '*' || c == '?' || c == '[';
  }

  inline bool CC_EQ(const char *class_ptr, int len, const char *litmatch)
  {
    return len == static_cast<int>(strlen(litmatch)) &&
           *class_ptr == *litmatch &&
           std::strncmp(class_ptr, litmatch, len) == 0;
  }

  int dowild(const char *p, const char *text, unsigned int flags)
  {
    char p_ch;
    const char *pattern = p;

    for (; (p_ch = *p) != '\0'; text++, p++)
    {
      char t_ch;
      int matched, match_slash, negated;
      char prev_ch;

      t_ch = *text;
      if (t_ch == '\0' && p_ch != '*')
        return WM_ABORT_ALL;

      if ((flags & WM_CASEFOLD) && ISUPPER(t_ch))
        t_ch = std::tolower(t_ch);
      if ((flags & WM_CASEFOLD) && ISUPPER(p_ch))
        p_ch = std::tolower(p_ch);

      switch (p_ch)
      {
      case '\\':
      {
        p_ch = *++p;
        if (p_ch == '\0')
          return WM_ABORT_ALL;
      }
      // FALLTHROUGH
      default:
        if (t_ch != p_ch)
          return WM_NOMATCH;
        continue;

      case '?':
        if ((flags & WM_PATHNAME) && t_ch == '/')
          return WM_NOMATCH;
        continue;

      case '*':
      {
        p++;
        if (*p == '*')
        {
          const char *prev_p = p;
          while (*++p == '*')
          {
          }
          if (!(flags & WM_PATHNAME))
          {
            match_slash = 1;
          }
          else if ((prev_p - pattern < 2 || *(prev_p - 2) == '/') &&
                   (*p == '\0' || *p == '/' ||
                    (p[0] == '\\' && p[1] == '/')))
          {
            if (p[0] == '/' && dowild(p + 1, text, flags) == WM_MATCH)
              return WM_MATCH;
            match_slash = 1;
          }
          else
          {
            match_slash = 0;
          }
        }
        else
        {
          match_slash = (flags & WM_PATHNAME) ? 0 : 1;
        }

        if (*p == '\0')
        {
          if (!match_slash)
          {
            if (std::strchr(text, '/'))
              return WM_ABORT_TO_STARSTAR;
          }
          return WM_MATCH;
        }
        else if (!match_slash && *p == '/')
        {
          const char *slash = std::strchr(text, '/');
          if (!slash)
            return WM_ABORT_ALL;
          text = slash;
          break;
        }

        while (1)
        {
          if (t_ch == '\0')
            break;

          if (!is_glob_special(*p))
          {
            p_ch = *p;
            if ((flags & WM_CASEFOLD) && ISUPPER(p_ch))
              p_ch = std::tolower(p_ch);

            while ((t_ch = *text) != '\0' &&
                   (match_slash || t_ch != '/'))
            {
              if ((flags & WM_CASEFOLD) && ISUPPER(t_ch))
                t_ch = std::tolower(t_ch);
              if (t_ch == p_ch)
                break;
              text++;
            }

            if (t_ch != p_ch)
            {
              if (match_slash)
                return WM_ABORT_ALL;
              else
                return WM_ABORT_TO_STARSTAR;
            }
          }

          matched = dowild(p, text, flags);
          if (matched != WM_NOMATCH)
          {
            if (!match_slash || matched != WM_ABORT_TO_STARSTAR)
              return matched;
          }
          else if (!match_slash && t_ch == '/')
          {
            return WM_ABORT_TO_STARSTAR;
          }

          t_ch = *++text;
        }
        return WM_ABORT_ALL;
      }

      case '[':
      {
        p_ch = *++p;
#ifdef NEGATE_CLASS2
        if (p_ch == NEGATE_CLASS2)
          p_ch = NEGATE_CLASS;
#endif
        negated = (p_ch == NEGATE_CLASS) ? 1 : 0;
        if (negated)
        {
          p_ch = *++p;
        }
        prev_ch = 0;
        matched = 0;

        do
        {
          if (!p_ch)
            return WM_ABORT_ALL;

          if (p_ch == '\\')
          {
            p_ch = *++p;
            if (!p_ch)
              return WM_ABORT_ALL;
            if (t_ch == p_ch)
              matched = 1;
          }
          else if (p_ch == '-' && prev_ch && p[1] && p[1] != ']')
          {
            p_ch = *++p;
            if (p_ch == '\\')
            {
              p_ch = *++p;
              if (!p_ch)
                return WM_ABORT_ALL;
            }
            if (t_ch <= p_ch && t_ch >= prev_ch)
              matched = 1;
            else if ((flags & WM_CASEFOLD) && ISLOWER(t_ch))
            {
              char t_ch_upper = std::toupper(t_ch);
              if (t_ch_upper <= p_ch && t_ch_upper >= prev_ch)
                matched = 1;
            }
            p_ch = 0;
          }
          else if (p_ch == '[' && p[1] == ':')
          {
            const char *s = p + 2;
            int i = 0;
            p += 2;
            while ((p_ch = *p) && p_ch != ']')
            {
              p++;
              i++;
            }
            if (!p_ch)
              return WM_ABORT_ALL;
            i = p - s - 1;

            if (i < 0 || p[-1] != ':')
            {
              p = s - 2;
              p_ch = '[';
              if (t_ch == p_ch)
                matched = 1;
              goto next_char_class;
            }

            if (CC_EQ(s, i, "alnum"))
            {
              if (ISALNUM(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "alpha"))
            {
              if (ISALPHA(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "blank"))
            {
              if (ISBLANK(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "cntrl"))
            {
              if (ISCNTRL(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "digit"))
            {
              if (ISDIGIT(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "graph"))
            {
              if (ISGRAPH(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "lower"))
            {
              if (ISLOWER(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "print"))
            {
              if (ISPRINT(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "punct"))
            {
              if (ISPUNCT(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "space"))
            {
              if (ISSPACE(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "upper"))
            {
              if (ISUPPER(t_ch))
                matched = 1;
              else if ((flags & WM_CASEFOLD) && ISLOWER(t_ch))
                matched = 1;
            }
            else if (CC_EQ(s, i, "xdigit"))
            {
              if (ISXDIGIT(t_ch))
                matched = 1;
            }
            else
            {
              return WM_ABORT_ALL;
            }
            p_ch = 0;
          }
          else if (t_ch == p_ch)
          {
            matched = 1;
          }

        next_char_class:
          prev_ch = p_ch;
          p_ch = *++p;
        } while (p_ch != ']');

        if (matched == negated ||
            ((flags & WM_PATHNAME) && t_ch == '/'))
          return WM_NOMATCH;
        continue;
      }
      }
    }

    return *text ? WM_NOMATCH : WM_MATCH;
  }


  // ---------------------------------------------------------------------
  // Compiled patterns
  //
  // Profiling the batch path showed dowild() dominating once the N-API
  // marshalling was fixed, and the overwhelming majority of real-world
  // pattern sets (.gitignore / .gitattributes) are two trivial shapes:
  // a plain literal, and "*" followed by a literal tail ("*.ts", "*/foo.c").
  // Recognising those once per call lets most (pattern, text) pairs be
  // decided with a memcmp instead of the recursive matcher.
  // ---------------------------------------------------------------------


  // ---------------------------------------------------------------------
  // String arena
  //
  // Utf8Value() heap-allocates one std::string per element, so a batch call
  // with T paths costs T mallocs before any matching happens. The arena takes
  // the lengths in one pass, allocates a single buffer, and copies every string
  // into it — one allocation total, and the texts end up contiguous in memory.
  // ---------------------------------------------------------------------
  struct StringArena
  {
    std::vector<char> buf;
    std::vector<uint32_t> offset; // n entries
    std::vector<uint32_t> length; // n entries

    const char *at(size_t i) const { return buf.data() + offset[i]; }
    size_t len(size_t i) const { return length[i]; }
  };

  // Returns false (with a pending JS TypeError) if any element is not a string.
  //
  // Two boundary crossings per element: fetch the value, then copy the UTF-8
  // bytes straight into the arena. There is deliberately no separate "ask for
  // the length" pass — the copy targets whatever room is left and only retries
  // (with a bigger arena) when the result could have been truncated.
  inline bool fill_arena(Napi::Env env, const Napi::Array &arr, uint32_t count,
                         StringArena &out, const char *errMsg)
  {
    out.offset.resize(count);
    out.length.resize(count);

    // 48 bytes per element covers the overwhelming majority of paths and
    // patterns; longer ones just trigger a growth step.
    size_t pos = 0;
    out.buf.resize(count ? count * 48 + 64 : 64);

    for (uint32_t i = 0; i < count; i++)
    {
      napi_value v = arr[i];

      while (true)
      {
        size_t avail = out.buf.size() - pos;
        if (avail < 64)
        {
          out.buf.resize(out.buf.size() * 2 + 64);
          continue;
        }
        size_t written = 0;
        napi_status st =
            napi_get_value_string_utf8(env, v, out.buf.data() + pos, avail, &written);
        if (st != napi_ok)
        {
          if (st == napi_string_expected)
            Napi::TypeError::New(env, errMsg).ThrowAsJavaScriptException();
          else
            Napi::Error::New(env, "Failed to read string").ThrowAsJavaScriptException();
          return false;
        }
        if (written == avail - 1)
        {
          // Might have been truncated (or fit exactly) — grow and redo to be sure.
          out.buf.resize(out.buf.size() * 2 + 64);
          continue;
        }
        out.offset[i] = static_cast<uint32_t>(pos);
        // dowild() is C-string based and stops at the first NUL, so the length
        // the fast paths use must stop there too — otherwise a text containing
        // a NUL would be matched differently by wildmatchMany than by wildmatch.
        out.length[i] =
            static_cast<uint32_t>(strnlen(out.buf.data() + pos, written));
        pos += written + 1;
        break;
      }
    }
    return true;
  }


  struct Lit
  {
    const char *p;
    uint32_t len;
  };

  struct PatternSet
  {
    std::vector<Lit> literals;          // no glob specials -> exact equality
    std::vector<Lit> suffixes;          // '*' + non-empty literal tail
    std::vector<const char *> generics; // everything else -> dowild()
    bool bareStar = false;              // the pattern "*" itself

    // Suffix patterns bucketed by the last byte of their tail. A text can only
    // be matched by suffixes whose tail ends in the text's own last byte, so a
    // text scans one bucket instead of every suffix pattern. Built with a
    // counting sort over 256 buckets — no per-pattern allocation.
    std::vector<Lit> bucketed;
    uint32_t bucketStart[257] = {0};
    bool indexed = false;

    // Generic patterns bucketed by their first byte when that byte is an
    // ordinary character: dowild() can only match a text starting with it.
    // Patterns beginning with a glob special go in the trailing "always"
    // bucket, which every text scans.
    std::vector<const char *> genBucketed;
    // Parallel to genBucketed: the byte the text must end with for this pattern
    // to have any chance, or 0 when the pattern imposes no such constraint.
    // Kept in its own array so the scan streams one byte per candidate and only
    // touches the 8-byte pointer for the few that survive.
    std::vector<unsigned char> genLastByte;
    uint32_t genBucketStart[258] = {0};
    bool genIndexed = false;
  };

  // If a pattern ends in an ordinary character, the whole pattern must consume
  // the whole text, so that character has to equal the text's last byte. Returns
  // 0 when the final element is a wildcard, a character class, or an escape,
  // i.e. when no such constraint can be derived.
  inline unsigned char required_last_byte(const char *p, size_t len)
  {
    if (!len)
      return 0;
    char c = p[len - 1];
    if (c == '*' || c == '?' || c == ']' || c == '\\')
      return 0;
    return static_cast<unsigned char>(c);
  }

  inline bool starts_with_special(const char *p, size_t len)
  {
    if (!len)
      return true;
    char c = p[0];
    return c == '*' || c == '?' || c == '[' || c == '\\';
  }

  inline bool has_glob_special(const char *s, size_t len)
  {
    for (size_t i = 0; i < len; i++)
    {
      char c = s[i];
      if (c == '*' || c == '?' || c == '[' || c == '\\')
        return true;
    }
    return false;
  }

  // Case-folding stays entirely on the generic path, so the fast kinds are only
  // ever chosen when they are exactly equivalent to dowild().
  inline void compile_into(PatternSet &set, const char *raw, size_t len, unsigned int flags)
  {
    if (flags & WM_CASEFOLD)
    {
      set.generics.push_back(raw);
      return;
    }

    if (!has_glob_special(raw, len))
    {
      set.literals.push_back({raw, static_cast<uint32_t>(len)});
      return;
    }

    if (raw[0] == '*' && !has_glob_special(raw + 1, len - 1))
    {
      if (len == 1)
        set.bareStar = true;
      else
        set.suffixes.push_back({raw + 1, static_cast<uint32_t>(len - 1)});
      return;
    }

    set.generics.push_back(raw);
  }

  // Bucketing costs ~256 counter operations to build and saves roughly
  // (patterns - patterns/buckets) comparisons per text, so it pays for itself
  // once patterns x texts is large enough. A fixed pattern-count threshold got
  // this wrong for small path lists, where the sort never amortises, and for
  // 50-pattern/200-path calls, where it very much does.
  inline bool index_worth_it(size_t patterns, size_t texts)
  {
    return patterns >= 4 && patterns * texts >= 1024;
  }

  inline void build_suffix_index(PatternSet &set, uint32_t tcount)
  {
    if (!index_worth_it(set.suffixes.size(), tcount))
      return;

    uint32_t counts[257] = {0};
    for (const Lit &l : set.suffixes)
      counts[static_cast<unsigned char>(l.p[l.len - 1]) + 1]++;
    for (int i = 1; i < 257; i++)
      counts[i] += counts[i - 1];
    for (int i = 0; i < 257; i++)
      set.bucketStart[i] = counts[i];

    set.bucketed.resize(set.suffixes.size());
    uint32_t cursor[256];
    for (int i = 0; i < 256; i++)
      cursor[i] = set.bucketStart[i];
    for (const Lit &l : set.suffixes)
      set.bucketed[cursor[static_cast<unsigned char>(l.p[l.len - 1])]++] = l;

    set.indexed = true;
  }

  inline void build_generic_index(PatternSet &set, unsigned int flags, uint32_t tcount)
  {
    if ((flags & WM_CASEFOLD) || !index_worth_it(set.generics.size(), tcount))
      return;

    // Bucket 256 is the "always scan" bucket.
    uint32_t counts[258] = {0};
    for (const char *g : set.generics)
    {
      size_t len = std::strlen(g);
      uint32_t b = starts_with_special(g, len)
                       ? 256u
                       : static_cast<unsigned char>(g[0]);
      counts[b + 1]++;
    }
    for (int i = 1; i < 258; i++)
      counts[i] += counts[i - 1];
    for (int i = 0; i < 258; i++)
      set.genBucketStart[i] = counts[i];

    set.genBucketed.resize(set.generics.size());
    set.genLastByte.resize(set.generics.size());
    uint32_t cursor[257];
    for (int i = 0; i < 257; i++)
      cursor[i] = set.genBucketStart[i];
    for (const char *g : set.generics)
    {
      size_t len = std::strlen(g);
      uint32_t b = starts_with_special(g, len)
                       ? 256u
                       : static_cast<unsigned char>(g[0]);
      uint32_t slot = cursor[b]++;
      set.genBucketed[slot] = g;
      set.genLastByte[slot] = required_last_byte(g, len);
    }

    set.genIndexed = true;
  }

  inline bool matches_any(const PatternSet &set, const char *text,
                          size_t textLen, unsigned int flags)
  {
    const bool pathname = (flags & WM_PATHNAME) != 0;

    if (set.bareStar && (!pathname || std::memchr(text, '/', textLen) == nullptr))
      return true;

    for (const Lit &l : set.literals)
    {
      if (textLen == l.len && std::memcmp(text, l.p, l.len) == 0)
        return true;
    }

    if (textLen)
    {
      unsigned char last = static_cast<unsigned char>(text[textLen - 1]);
      const Lit *begin;
      const Lit *end;
      if (set.indexed)
      {
        begin = set.bucketed.data() + set.bucketStart[last];
        end = set.bucketed.data() + set.bucketStart[last + 1];
      }
      else
      {
        begin = set.suffixes.data();
        end = begin + set.suffixes.size();
      }
      for (const Lit *l = begin; l != end; ++l)
      {
        if (textLen < l->len)
          continue;
        size_t head = textLen - l->len;
        if (std::memcmp(text + head, l->p, l->len) != 0)
          continue;
        // With WM_PATHNAME the leading '*' may not span a '/'.
        if (pathname && std::memchr(text, '/', head) != nullptr)
          continue;
        return true;
      }
    }

    if (!set.genIndexed)
    {
      for (const char *g : set.generics)
      {
        if (dowild(g, text, flags) == WM_MATCH)
          return true;
      }
      return false;
    }

    unsigned char textLast =
        textLen ? static_cast<unsigned char>(text[textLen - 1]) : 0;

    // Scans the required-last-byte array (1 byte per candidate) and only calls
    // the recursive matcher for the handful that can still match.
    auto scanGeneric = [&](uint32_t from, uint32_t to) -> bool {
      const unsigned char *req = set.genLastByte.data();
      const char *const *pat = set.genBucketed.data();
      for (uint32_t i = from; i < to; i++)
      {
        if (req[i] && req[i] != textLast)
          continue;
        if (dowild(pat[i], text, flags) == WM_MATCH)
          return true;
      }
      return false;
    };

    if (textLen)
    {
      unsigned char first = static_cast<unsigned char>(text[0]);
      if (scanGeneric(set.genBucketStart[first], set.genBucketStart[first + 1]))
        return true;
    }
    return scanGeneric(set.genBucketStart[256], set.genBucketStart[257]);
  }

  Napi::Boolean WildMatch(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString())
    {
      Napi::TypeError::New(env, "Expected (pattern: string, text: string, flags?: number)")
          .ThrowAsJavaScriptException();
      return Napi::Boolean::New(env, false);
    }

    std::string pattern = info[0].As<Napi::String>().Utf8Value();
    std::string text = info[1].As<Napi::String>().Utf8Value();
    unsigned int flags = 0;

    if (info.Length() > 2 && info[2].IsNumber())
    {
      flags = info[2].As<Napi::Number>().Uint32Value();
    }

    int result = dowild(pattern.c_str(), text.c_str(), flags);
    return Napi::Boolean::New(env, result == WM_MATCH);
  }

  Napi::Number WildMatchPos(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString())
    {
      Napi::TypeError::New(env, "Expected (pattern: string, text: string, flags?: number)")
          .ThrowAsJavaScriptException();
      return Napi::Number::New(env, -1);
    }

    std::string pattern = info[0].As<Napi::String>().Utf8Value();
    std::string text = info[1].As<Napi::String>().Utf8Value();
    unsigned int flags = 0;

    if (info.Length() > 2 && info[2].IsNumber())
    {
      flags = info[2].As<Napi::Number>().Uint32Value();
    }

    int result = dowild(pattern.c_str(), text.c_str(), flags);
    // WM_ABORT_* are internal control values; callers only ever see
    // WM_MATCH / WM_NOMATCH.
    return Napi::Number::New(env, result == WM_MATCH ? WM_MATCH : WM_NOMATCH);
  }

  // Shared sweep: compile, index, match (in parallel when it pays), and return
  // the matching text indices as a Uint32Array.
  inline Napi::Value run_many(Napi::Env env, const StringArena &patArena, uint32_t pcount,
                              const StringArena &textArena, uint32_t tcount,
                              unsigned int flags)
  {
    PatternSet set;
    set.literals.reserve(pcount);
    set.suffixes.reserve(pcount);
    for (uint32_t pi = 0; pi < pcount; pi++)
      compile_into(set, patArena.at(pi), patArena.len(pi), flags);
    build_suffix_index(set, tcount);
    build_generic_index(set, flags, tcount);

    std::vector<uint32_t> matchedIdx;
    for (uint32_t ti = 0; ti < tcount; ti++)
    {
      if (matches_any(set, textArena.at(ti), textArena.len(ti), flags))
        matchedIdx.push_back(ti);
    }

    // Hand back the *indices* of the matching texts, not the strings. Copying
    // one typed array is a single boundary crossing; setting m strings was two
    // crossings per match (get the original value, set it on the result array).
    Napi::Uint32Array results = Napi::Uint32Array::New(env, matchedIdx.size());
    if (!matchedIdx.empty())
    {
      std::memcpy(results.Data(), matchedIdx.data(),
                  matchedIdx.size() * sizeof(uint32_t));
    }
    return results;
  }

  Napi::Value WildMatchMany(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();

    if (info.Length() < 3 ||
        !info[0].IsArray() || !info[1].IsArray() || !info[2].IsNumber())
    {
      Napi::TypeError::New(env, "Expected (patterns: string[], texts: string[], flags: number)")
          .ThrowAsJavaScriptException();
      return Napi::Array::New(env, 0);
    }

    Napi::Array patterns = info[0].As<Napi::Array>();
    Napi::Array texts = info[1].As<Napi::Array>();
    unsigned int flags = info[2].As<Napi::Number>().Uint32Value();

    uint32_t pcount = patterns.Length();
    uint32_t tcount = texts.Length();

    StringArena textArena;
    if (!fill_arena(env, texts, tcount, textArena, "All texts must be strings"))
      return Napi::Array::New(env, 0);

    StringArena patArena;
    if (!fill_arena(env, patterns, pcount, patArena, "All patterns must be strings"))
      return Napi::Array::New(env, 0);

    return run_many(env, patArena, pcount, textArena, tcount, flags);
  }

  Napi::Object Init(Napi::Env env, Napi::Object exports)
  {
    exports.Set(Napi::String::New(env, "wildmatch"),
                Napi::Function::New(env, WildMatch));
    exports.Set(Napi::String::New(env, "wildmatchPos"),
                Napi::Function::New(env, WildMatchPos));
    exports.Set(Napi::String::New(env, "wildmatchMany"),
                Napi::Function::New(env, WildMatchMany));

    exports.Set(Napi::String::New(env, "WM_CASEFOLD"),
                Napi::Number::New(env, WM_CASEFOLD));
    exports.Set(Napi::String::New(env, "WM_PATHNAME"),
                Napi::Number::New(env, WM_PATHNAME));
    exports.Set(Napi::String::New(env, "WM_MATCH"),
                Napi::Number::New(env, WM_MATCH));
    exports.Set(Napi::String::New(env, "WM_NOMATCH"),
                Napi::Number::New(env, WM_NOMATCH));

    return exports;
  }

  NODE_API_MODULE(wildmatch, Init)

}
