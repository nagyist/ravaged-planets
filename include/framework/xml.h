#pragma once

#include <boost/lexical_cast.hpp>

#include <framework/exception.h>
#include <framework/tinyxml2.h>

#define XML_CHECK(fn) { \
  ::fw::xml::XMLError err = fn; \
  if (err != ::fw::xml::XML_NO_ERROR) { \
    BOOST_THROW_EXCEPTION(fw::exception() << fw::xml_error_info(err)); \
  } \
}

namespace fw {
// import the tinyxml2 namespace into our own.
namespace xml = tinyxml2;

typedef boost::error_info<struct tag_xmlerror, fw::xml::XMLError> xml_error_info;
std::string to_string(xml_error_info const &err_info);

/** This is a wrapper around the TiXmlElement which we use to simplify various things we like to do. */
class xml_element {
private:
  std::shared_ptr<xml::XMLDocument> _doc;
  xml::XMLElement *_elem;

public:
  xml_element();
  xml_element(std::shared_ptr<xml::XMLDocument> doc, xml::XMLElement *elem);
  xml_element(xml_element const &copy);
  xml_element(std::string const &xml);
  ~xml_element();

  xml_element &operator =(xml_element const &copy);

  std::shared_ptr<xml::XMLDocument> get_document() const;
  xml::XMLElement *get_element() const;
  bool is_valid() const;

  std::string get_value() const;
  std::string get_text() const;
  std::string get_attribute(std::string const &name) const;

  template <typename T>
  inline T get_attribute(std::string const &name) const {
    return boost::lexical_cast<T>(get_attribute(name));
  }

  bool is_attribute_defined(std::string const &name) const;

  xml_element get_first_child() const;
  xml_element get_next_sibling() const;
};

/**
 * This is a simple helper that loads an XML document and verifies the basic properties. If it can't, it'll return 0
 * and print a diagnostic error to the log file as well.
 *
 * \param format_name defines what we log (for debugging) and is also expected to be the name of the root XML element.
 * \param version The root XML element is expected to have a "version" attribute with a value corresponding to this.
 */
xml_element load_xml(boost::filesystem::path const &filepath, std::string const &format_name, int version);
}


