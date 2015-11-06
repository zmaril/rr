/* -*- Mode: C++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include <algorithm>
#include <assert.h>
#include <vector>

#include "Command.h"
#include "dirent.h"
#include "main.h"
#include "TraceStream.h"
#include "util.h"

using namespace std;

class LsCommand : public Command {
public:
  virtual int run(vector<string>& args);

protected:
  LsCommand(const char* name, const char* help) : Command(name, help) {}

  static LsCommand singleton;
};

LsCommand LsCommand::singleton(
    "ls", " rr ls [OPTION]...\n"
          "  -l, --long-listing use a long listing format \n "
          "     (trace name | command line | start time | duration | size)\n"
          "  -t, --sort-by-age, sort from newest to oldest\n"
          "  -r, --reverse, the sort order\n");

struct LsFlags {
  bool reverse;
  bool full_listing;
  bool sort_by_time;
  LsFlags() : reverse(false), full_listing(false), sort_by_time(false) {}
};

static bool parse_ls_arg(vector<string>& args, LsFlags& flags) {
  if (parse_global_option(args)) {
    return true;
  }

  static const OptionSpec options[] = { { 'r', "reverse", NO_PARAMETER },
                                        { 'l', "long-listing", NO_PARAMETER },
                                        { 't', "sort-by-age", NO_PARAMETER } };
  ParsedOption opt;
  if (!Command::parse_option(args, options, &opt)) {
    return false;
  }

  switch (opt.short_name) {
    case 'r':
      flags.reverse = true;
      break;
    case 'l':
      flags.full_listing = true;
      break;
    case 't':
      flags.sort_by_time = true;
      break;
    default:
      assert(0 && "Unknown option");
  }

  return true;
}

typedef pair<dirent*, TraceReader*> trace_info;

bool compare_by_name(trace_info at, trace_info bt) {
  auto a = string(at.first->d_name);
  auto b = string(bt.first->d_name);
  return lexicographical_compare(begin(a), end(a), begin(b), end(b));
}

bool compare_by_time(trace_info at, trace_info bt) {
  auto a_version = at.second->dir() + "/version";
  auto b_version = bt.second->dir() + "/version";
  struct stat a_stat;
  struct stat b_stat;
  stat(a_version.c_str(), &a_stat);
  stat(b_version.c_str(), &b_stat);
  return a_stat.st_ctime < b_stat.st_ctime;
}

// http://stackoverflow.com/questions/15495756/how-to-find-the-size-of-all-files-located-inside-a-folder
// Blatantly copied from above, slightly modified
string get_folder_size(string path) {
  // command to be executed
  string cmd("du -sh " + path + " | cut -f1 2>&1 | tr -d '\\n'");
  FILE* stream = popen(cmd.c_str(), "r");
  if (stream) {
    const int max_size = 256;
    char* readbuf = new char[max_size];
    if (fgets(readbuf, max_size, stream) != NULL) {
      return string(readbuf);
    }
    pclose(stream);
  }
  // return error val
  return "ERROR";
}

static int ls(const string traces_dir, const LsFlags& flags) {
  if (DIR* dir = opendir(traces_dir.c_str())) {
    vector<trace_info> traces;

    while (struct dirent* trace_dir = readdir(dir)) {
      if (strcmp(trace_dir->d_name, ".") == 0)
        continue;
      if (strcmp(trace_dir->d_name, "..") == 0)
        continue;
      string full_trace_dir = traces_dir + "/" + trace_dir->d_name;
      traces.emplace_back(trace_dir, new TraceReader(full_trace_dir));
    }

    sort(traces.begin(), traces.end(),
         flags.sort_by_time ? compare_by_time : compare_by_name);

    if (flags.reverse) {
      reverse(begin(traces), end(traces));
    };

    if (flags.full_listing) {
      int max_name_size =
          accumulate(traces.begin(), traces.end(), 0, [](int m, trace_info t) {
            return max(m, static_cast<int>(strlen(t.first->d_name)));
          });

      for_each(traces.begin(), traces.end(), [&](trace_info t) {
        // Record date & runtime estimates
        struct stat stat_version;
        struct stat stat_data;
        string version_file = traces_dir + "/" + t.first->d_name + "/version";
        string data_file = traces_dir + "/" + t.first->d_name + "/data";
        stat(version_file.c_str(), &stat_version);
        stat(data_file.c_str(), &stat_data);
        long int difference = stat_data.st_ctime - stat_version.st_ctime;
        char outstr[200];
        strftime(outstr, sizeof(outstr), "%b %d %k:%M",
                 localtime(&stat_version.st_ctime));
        const char* cmdl = t.second->initial_exe().c_str();
        string folder_size = get_folder_size(t.second->dir());
        fprintf(stdout, "%-*s %s %li %s %s\n", max_name_size, t.first->d_name,
                outstr, difference, folder_size.c_str(), cmdl);
      });
    } else {
      for_each(traces.begin(), traces.end(),
               [](trace_info t) { cout << t.first->d_name << " "; });
      fprintf(stdout, "\n");
    }
    return 0;
  } else {
    fprintf(stdout, "Cannot open %s", traces_dir.c_str());
    return 1;
  }
}

int LsCommand::run(vector<string>& args) {
  if (getenv("RUNNING_UNDER_RR")) {
    fprintf(stderr, "rr: cannot run rr replay under rr. Exiting.\n");
    return 1;
  }

  bool found_dir = false;
  string trace_dir;
  LsFlags flags;

  while (!args.empty()) {
    if (parse_ls_arg(args, flags)) {
      continue;
    }
    if (!found_dir && parse_optional_trace_dir(args, &trace_dir)) {
      found_dir = true;
      continue;
    }
    print_help(stderr);
    return 1;
  };

  if (!found_dir) {
    trace_dir = trace_save_dir();
  }
  return ls(trace_dir, flags);
};