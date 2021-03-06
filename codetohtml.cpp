#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#include "codetohtml.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include <boost/scoped_ptr.hpp>
#include <boost/xpressive/xpressive.hpp>

#include <QDir>
#include <QFile>

#include "codetohtmlfile.h"
#include "codetohtmlfooter.h"
#include "codetohtmlheader.h"
#include "codetohtmlreplacer.h"

#include "codetohtmlsnippettype.h"
#include "codetohtmltechinfo.h"
#include "fileio.h"
#include "qtcreatorprofile.h"
#include "qtcreatorprofilezipscript.h"


#pragma GCC diagnostic pop

#ifndef _WIN32
bool ribi::c2h::IsCleanHtml(const std::vector<std::string>& html)
{
  assert(IsTidyInstalled());

  const std::string temp_filename { fileio::FileIo().GetTempFileName(".htm") };
  const std::string temp_filename_tidy { fileio::FileIo().GetTempFileName("_tidy.txt") };
  assert(!ribi::fileio::FileIo().IsRegularFile(temp_filename));
  //Write HTML to file
  {
    std::ofstream f(temp_filename.c_str());
    std::copy(html.begin(),html.end(),std::ostream_iterator<std::string>(f,"\n"));
  }
  //Start tidy, creates output file
  {
    assert(ribi::fileio::FileIo().IsRegularFile(temp_filename));
    const std::string command = "tidy -q -e -f " + temp_filename_tidy + " " + temp_filename;
    const int error = std::system(command.c_str());
    if (error) return false;
  }
  const auto v = ribi::fileio::FileIo().FileToVector(temp_filename_tidy);
  if (v.size() > 1)
  {
    std::cerr << "Errors found by Tidy, check the following files:";
    std::cerr << temp_filename;
    std::cerr << temp_filename_tidy;
    return false;
  }
  fileio::FileIo().DeleteFile(temp_filename.c_str());
  fileio::FileIo().DeleteFile(temp_filename_tidy.c_str());
  return true;
}
#endif

#ifndef _WIN32
bool ribi::c2h::IsTidyInstalled()
{
  const std::string temp_filename_tidy { fileio::FileIo().GetTempFileName() };

  assert(!ribi::fileio::FileIo().IsRegularFile(temp_filename_tidy));

  //'2>' denotes -AFAIK- 'Write to file only, no screen output'
  const std::string command = "tidy -v 2> ./" + temp_filename_tidy;
  const int error = std::system(command.c_str());
  //assert(ok == 0 && "While I know I have tidy installed");
  //assert(IsRegularFile(temp_filename_tidy) && "While I know I have tidy installed");
  if (!ribi::fileio::FileIo().IsRegularFile(temp_filename_tidy)) return false;

  fileio::FileIo().DeleteFile(temp_filename_tidy.c_str());
  assert(!ribi::fileio::FileIo().IsRegularFile(temp_filename_tidy));

  return !error;
}
#endif

std::vector<std::string> ribi::c2h::GetSortedFilesInFolder(const std::string& folder) noexcept
{
  std::vector<std::string> files {
    FilterFiles(
      ribi::fileio::FileIo().GetFilesInFolder(folder)
    )
  };
  files = SortFiles(files);
  return files;
}

std::vector<std::string> ribi::c2h::FilterFiles(const std::vector<std::string>& files) noexcept
{
  std::vector<std::string> v;
  std::copy_if(files.begin(), files.end(),std::back_inserter(v),
    [](const std::string& file)
    {
      if (file.size() >= 3)
      {
        if (file.substr(0,3) == "ui_") return false;
      }
      if (file.size() >= 4)
      {
        if (file.substr(0,4) == "qrc_") return false;
        if (file.substr(0,4) == "moc_") return false;
      }
      if (file.size() >= 18)
      {
        if (file.substr(file.size() - 18, 18) ==  "_plugin_import.cpp") return false;
      }
      const std::string ext = ribi::fileio::FileIo().GetExtensionNoDot(file);
      return
           ext == "c"
        || ext == "cpp"
        || ext == "h"
        || ext == "hpp"
        || ext == "pri"
        || ext == "pro"
        || ext == "py"
        || ext == "sh";
    }
  );
  return v;
}


std::vector<std::string> ribi::c2h::SortFiles(std::vector<std::string> files) noexcept
{
  std::sort(files.begin(), files.end(),
    [](const std::string& lhs,const std::string& rhs)
    {
      const std::string lhs_base = ribi::fileio::FileIo().GetFileBasename(lhs);
      const std::string rhs_base = ribi::fileio::FileIo().GetFileBasename(rhs);
      const std::string lhs_ext = ribi::fileio::FileIo().GetExtension(lhs);
      const std::string rhs_ext = ribi::fileio::FileIo().GetExtension(rhs);
      static std::string pro(".pro");
      static std::string pri(".pri");
      static std::string sh(".sh");
      static std::string h(".h");
      static std::string cpp(".cpp");
      static std::string py(".py");
      if (lhs_ext == pro)
      {
        //.pro files first
        if (rhs_ext != pro)
        {
          return true;
        }
        if (rhs_ext == pro)
        {
          return lhs_base < rhs_base;
        }
      }

      //.pri files then
      if (lhs_ext == pri)
      {
        if (rhs_ext == pro) //.pro first
        {
          return false;
        }
        if (rhs_ext == pri) //sort .pri files
        {
          return lhs_base < rhs_base;
        }
        if (rhs_ext != pri) //else .pri is second
        {
          return true;
        }
      }

      //Headers then
      if (lhs_ext == h && rhs_ext == cpp && lhs_base == rhs_base)
      {
        return true;
      }
      //Pri before unit files
      if ( (lhs_ext == h || lhs_ext == cpp) && rhs_ext == pri)
      {
        return false;
      }

      //Unit files before script files
      if ( (lhs_ext == h || lhs_ext == cpp) && rhs_ext == sh)
      {
        return true;
      }
      //Sort unit files alphabeticaly
      if ( (lhs_ext == h || lhs_ext == cpp) && (rhs_ext == h || rhs_ext == cpp) )
      {
        return lhs_base < rhs_base;
      }

      //.py one-but-last
      if (lhs_ext == py)
      {
        if (rhs_ext == sh)
        {
          return true;
        }
        if (rhs_ext != py)
        {
          return false;
        }
        if (rhs_ext == py)
        {
          return lhs_base < rhs_base;
        }
      }


      //.sh last
      if (lhs_ext == sh)
      {
        if (rhs_ext != sh)
        {
          return false;
        }
        if (rhs_ext == sh)
        {
          return lhs_base < rhs_base;
        }
      }
      return false;
    }
  );
  return files;
}
