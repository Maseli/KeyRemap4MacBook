#include <exception>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include "pqrs/string.hpp"
#include "pqrs/xml_compiler.hpp"

namespace pqrs {
  void
  xml_compiler::reload(void)
  {
    error_information_.clear();

    try {
      reload_replacementdef_();
      reload_symbol_map_();
      reload_appdef_();
      reload_devicedef_();
      reload_autogen_();
      reload_preferences_();
    } catch (std::exception& e) {
      error_information_.set(e.what());
    }
  }

  void
  xml_compiler::read_xml_(ptree_ptr& out,
                          const std::string& file_path,
                          const pqrs::string::replacement& replacement)
  {
    out.reset(new boost::property_tree::ptree());

    std::string xml;
    if (replacement.empty()) {
      pqrs::string::string_from_file(xml, file_path.c_str());
    } else {
      pqrs::string::string_by_replacing_double_curly_braces_from_file(xml, file_path.c_str(), replacement);
    }
    std::stringstream istream(xml, std::stringstream::in);

    int flags = boost::property_tree::xml_parser::no_comments;

    boost::property_tree::read_xml(istream, *out, flags);
  }

  void
  xml_compiler::read_xmls_(std::vector<ptree_ptr>& pt_ptrs, const std::vector<xml_file_path_ptr>& xml_file_path_ptrs)
  {
    pt_ptrs.clear();

    for (auto& path_ptr : xml_file_path_ptrs) {
      try {
        ptree_ptr pt_ptr;

        std::string path;
        switch (path_ptr->get_base_directory()) {
          case xml_file_path::base_directory::system_xml:
            path += system_xml_directory_;
            break;
          case xml_file_path::base_directory::private_xml:
            path += private_xml_directory_;
            break;
        }
        path += "/";
        path += path_ptr->get_relative_path();

        read_xml_(pt_ptr, path, replacement_);
        if (pt_ptr) {
          pt_ptrs.push_back(pt_ptr);
        }

      } catch (std::exception& e) {
        std::string what = e.what();

        // Hack:
        // boost::property_tree::read_xml throw exception with filename.
        // But, when we call read_xml with stream, the filename becomes "unspecified file" as follow.
        //
        // <unspecified file>(4): expected element name
        //
        // So, we change "unspecified file" to file name by ourself.
        boost::replace_first(what, "<unspecified file>", std::string("<") + path_ptr->get_relative_path() + ">");

        error_information_.set(what);
      }
    }
  }

  boost::optional<const std::string&>
  xml_compiler::get_identifier(int config_index) const
  {
    auto it = identifier_map_.find(config_index);
    if (it == identifier_map_.end()) {
      return boost::none;
    }
    return it->second;
  }

  uint32_t
  xml_compiler::get_appid(const std::string& application_identifier) const
  {
    for (auto& it : app_) {
      if (! it) continue;

      if (it->is_rules_matched(application_identifier)) {
        auto name = it->get_name();
        if (! name) goto notfound;

        auto v = symbol_map_.get_optional(std::string("ApplicationType::") + *name);
        if (! v) goto notfound;

        return *v;
      }
    }

  notfound:
    // return ApplicationType::UNKNOWN (== 0)
    return 0;
  }

  void
  xml_compiler::normalize_identifier(std::string& identifier)
  {
    boost::replace_all(identifier, ".", "_");
  }

  void
  xml_compiler::extract_include_(ptree_ptr& out,
                                 const boost::property_tree::ptree::value_type& it)
  {
    out.reset();

    if (it.first != "include") return;

    out.reset(new boost::property_tree::ptree());

    // ----------------------------------------
    // file_path
    auto path = it.second.get_optional<std::string>("<xmlattr>.path");
    if (! path) return;

    if (! boost::starts_with("/", *path)) {
      path = private_xml_directory_ + "/" + *path;
    }

    // ----------------------------------------
    // replacement
    pqrs::string::replacement r;
    traverse_replacementdef_(it.second, r);

    for (auto& i : replacement_) {
      if (r.find(i.first) == r.end()) {
        r[i.first] = i.second;
      }
    }

    // ----------------------------------------
    read_xml_(out, *path, r);
  }
}