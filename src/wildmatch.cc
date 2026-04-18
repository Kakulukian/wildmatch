#include <napi.h>
#include <cstring>
#include <cctype>

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
    return Napi::Number::New(env, result);
  }

  Napi::Array WildMatchMany(const Napi::CallbackInfo &info)
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

    std::vector<std::string> matchedTexts;
    std::vector<bool> textMatched(tcount, false);

    for (uint32_t pi = 0; pi < pcount; pi++)
    {
      Napi::Value patVal = patterns[pi];
      if (!patVal.IsString())
      {
        Napi::TypeError::New(env, "All patterns must be strings")
            .ThrowAsJavaScriptException();
        return Napi::Array::New(env, 0);
      }
      std::string pattern = patVal.As<Napi::String>().Utf8Value();

      for (uint32_t ti = 0; ti < tcount; ti++)
      {
        if (textMatched[ti]) continue;

        Napi::Value txtVal = texts[ti];
        if (!txtVal.IsString())
        {
          Napi::TypeError::New(env, "All texts must be strings")
              .ThrowAsJavaScriptException();
          return Napi::Array::New(env, 0);
        }
        std::string text = txtVal.As<Napi::String>().Utf8Value();
        if (dowild(pattern.c_str(), text.c_str(), flags) == WM_MATCH)
        {
          textMatched[ti] = true;
          matchedTexts.push_back(text);
        }
      }
    }

    Napi::Array results = Napi::Array::New(env, matchedTexts.size());
    for (size_t i = 0; i < matchedTexts.size(); i++)
    {
      results.Set(i, Napi::String::New(env, matchedTexts[i]));
    }
    return results;
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
