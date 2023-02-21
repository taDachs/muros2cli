#include <dirent.h>
#include <libgen.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define DEBUG

#ifdef DEBUG
#  define _DEBUG(fmt, args...) printf("%s:%s:%d: " fmt, __FILE__, __FUNCTION__, __LINE__, args)
#else
#  define _DEBUG(fmt, args...)
#endif

#define errorf(fmt, args...)                                                                       \
  printf(fmt, args);                                                                               \
  exit(1)
#define error(fmt)                                                                                 \
  printf(fmt);                                                                                     \
  exit(1)

#define PKG_BUFFER_SIZE 10000
#define STRING_BUFFER_SIZE 4096 // maximal path length under unix

#define AMENT_PREFIX_PATH "AMENT_PREFIX_PATH"
#define INSTALL_DIR "/install/"
#define OPT_ROS "/opt/ros/"
#define SRC "src"
#define PACKAGE_XML "package.xml"
#define PATH_SEP "/"
#define PACKAGE_NAME_REGEX "<name>\\(\\w\\+\\)</name>"

#define PATH_VERB "path"
#define LIST_PATH_VERB "list-paths"
#define LIST_VERB "list"
#define FIND_VERB "find"

typedef struct
{
  char path[STRING_BUFFER_SIZE];
  char name[STRING_BUFFER_SIZE];
} package;

static package pkgs[PKG_BUFFER_SIZE];
static size_t num_pkgs = 0;

int foundPackage(const char* name)
{
  for (int i = 0; i < num_pkgs; ++i)
  {
    if (strcmp(pkgs[i].name, name) == 0)
    {
      return 1;
    }
  }
  return 0;
}

typedef struct
{
  char* ament_prefix_path;
  char previous_workspace[4096];
} workspaceIt;


workspaceIt* getWorkspaceIt()
{
  char* ament_prefix_path = getenv(AMENT_PREFIX_PATH);

  if (ament_prefix_path == NULL)
  {
    return NULL;
  }

  workspaceIt* it           = (workspaceIt*)malloc(sizeof(workspaceIt));
  it->ament_prefix_path     = ament_prefix_path;
  it->previous_workspace[0] = 0;
  return it;
}

void closeIt(workspaceIt* it)
{
  free(it);
}

int readPackageNameFromFile(const char* path, char* dst)
{
  _DEBUG("Reading from path: %s\n", path);
  FILE* fp = fopen(path, "r");

  if (fp == NULL)
  {
    return 1;
  }

  regex_t regex;
  int reti = regcomp(&regex, PACKAGE_NAME_REGEX, 0);
  if (reti)
  {
    errorf("could not compile regex: %s\n", PACKAGE_NAME_REGEX);
  }

  regmatch_t pmatch[2];
  char line[1000];
  while ((fgets(line, 1000, fp)))
  {
    reti = regexec(&regex, line, 2, pmatch, 0);
    if (!reti)
    {
      char* name;
      name                  = line + pmatch[1].rm_so;
      line[pmatch[1].rm_eo] = 0;
      strcpy(dst, name);
      break;
    }
  }

  fclose(fp);
  regfree(&regex);

  return 0;
}

char* getWorkspaceRoot(workspaceIt* it)
{
  if (it == NULL || it->ament_prefix_path == NULL)
  {
    return NULL;
  }

  _DEBUG("current prefix path: %s\n", it->ament_prefix_path);

  size_t path_len = strcspn(it->ament_prefix_path, ":");
  if (path_len == 0)
  {
    return NULL;
  }
  char next_package_path[path_len + 1];
  strncpy(next_package_path, it->ament_prefix_path, path_len);
  next_package_path[path_len] = 0;
  it->ament_prefix_path       = it->ament_prefix_path + path_len;
  if (strlen(it->ament_prefix_path) != 0)
  {
    it->ament_prefix_path++;
  }

  // package in workspace
  char* install_dir_substr = strstr(next_package_path, INSTALL_DIR);
  if (install_dir_substr != NULL)
  {
    size_t install_dir_offset = install_dir_substr - next_package_path;

    char root[install_dir_offset + strlen(PATH_SEP) + strlen(SRC) + 1];
    strncpy(root, next_package_path, install_dir_offset);
    root[install_dir_offset] = 0;
    strcat(root, PATH_SEP);
    strcat(root, SRC);
    if (strcmp(root, it->previous_workspace) == 0)
    {
      return getWorkspaceRoot(it);
    }
    strcpy(it->previous_workspace, root);
    return it->previous_workspace;
  }

  // global ros package
  char* opt_ros_substr = strstr(next_package_path, OPT_ROS);
  if (opt_ros_substr != NULL)
  {
    char root[strlen(next_package_path) + 1];
    strcpy(root, next_package_path);
    if (strcmp(root, it->previous_workspace) == 0)
    {
      return getWorkspaceRoot(it);
    }
    strcpy(it->previous_workspace, root);
    return it->previous_workspace;
  }

  return NULL;
}


int crawl(char* root)
{
  DIR* d = opendir(root);
  _DEBUG("Crawling through dir: %s\n", root);

  if (d == NULL)
  {
    return 1;
  }

  struct dirent* dir;
  while ((dir = readdir(d)) != NULL)
  {
    // ignore hidden files, and . / ..
    if (dir->d_name[0] == '.')
    {
      continue;
    }

    if (strcmp(dir->d_name, PACKAGE_XML) == 0)
    {
      char package_xml_path[strlen(root) + strlen(PATH_SEP) + strlen(PACKAGE_XML)];
      strcpy(package_xml_path, root);
      strcat(package_xml_path, PATH_SEP);
      strcat(package_xml_path, PACKAGE_XML);
      char pkg_name[200];
      readPackageNameFromFile(package_xml_path, pkg_name);

      if (foundPackage(pkg_name))
      {
        _DEBUG("Package already exists: %s\n", root);
        continue;
      }

      _DEBUG("Found package at %s\n", root);

      strcpy(pkgs[num_pkgs].path, root);
      strcpy(pkgs[num_pkgs].name, pkg_name);
      num_pkgs++;
      continue;
    }

    if (dir->d_type == DT_DIR)
    {
      char subdir[strlen(root) + strlen(PATH_SEP) + strlen(dir->d_name) + 1];
      strcpy(subdir, root);
      strcat(subdir, PATH_SEP);
      strcat(subdir, dir->d_name);
      crawl(subdir);
    }
  }

  closedir(d);
  return 0;
}

int handlePath(int argc, char** argv)
{
  workspaceIt* ws_it = getWorkspaceIt();
  if (ws_it == NULL)
  {
    error("No workspace sourced.\n");
  }
  char* ws_root = getWorkspaceRoot(ws_it);
  ws_root       = dirname(ws_root);

  printf("%s\n", ws_root);
  closeIt(ws_it);
  return 0;
}

int handleListPaths(int argc, char** argv)
{
  workspaceIt* ws_it = getWorkspaceIt();
  if (ws_it == NULL)
  {
    error("No workspace sourced.\n");
  }
  char* ws_root;

  while ((ws_root = getWorkspaceRoot(ws_it)))
  {
    char ws_root_cpy[strlen(ws_root) + 1];
    strcpy(ws_root_cpy, ws_root);
    ws_root = dirname(ws_root_cpy);
    printf("%s\n", ws_root);
  }
  closeIt(ws_it);
  return 0;
}

int handleFind(int argc, char** argv)
{
  if (argc < 3)
  {
    error("No package name given");
  }
  workspaceIt* ws_it = getWorkspaceIt();
  if (ws_it == NULL)
  {
    error("No workspace sourced.\n");
  }
  char* ws_root;
  while ((ws_root = getWorkspaceRoot(ws_it)))
  {
    crawl(ws_root);
  }

  int found = 0;
  for (int i = 0; i < num_pkgs; ++i)
  {
    if (strcmp(pkgs[i].name, argv[2]) == 0)
    {
      printf("%s\n", pkgs[i].path);
      found = 1;
      break;
    }
  }

  closeIt(ws_it);

  if (!found)
  {
    errorf("%s doesn't exist\n", argv[2]);
    return 1;
  }

  return 0;
}

int handleList(int argc, char** argv)
{
  workspaceIt* ws_it = getWorkspaceIt();
  if (ws_it == NULL)
  {
    error("No workspace sourced.\n");
  }

  char* ws_root;
  while ((ws_root = getWorkspaceRoot(ws_it)))
  {
    crawl(ws_root);
  }

  for (int i = 0; i < num_pkgs; ++i)
  {
    printf("%s %s\n", pkgs[i].name, pkgs[i].path);
  }
  closeIt(ws_it);

  return 0;
}

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    error("Needs a verb\n");
  }

  if (strcmp(argv[1], PATH_VERB) == 0)
  {
    return handlePath(argc, argv);
  }

  if (strcmp(argv[1], LIST_PATH_VERB) == 0)
  {
    return handleListPaths(argc, argv);
  }

  if (strcmp(argv[1], FIND_VERB) == 0)
  {
    return handleFind(argc, argv);
  }

  if (strcmp(argv[1], LIST_VERB) == 0)
  {
    return handleList(argc, argv);
  }


  errorf("Unknown verb: %s\n", argv[1]);
}
