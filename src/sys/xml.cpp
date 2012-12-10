#define RAPIDXML_NO_STREAMS
#include "rapidxml-1.13/rapidxml.hpp"
#include "rapidxml-1.13/rapidxml_print.hpp"
#include "sys/xml.h"
#include <sstream>
#include <cstring>
#include <cstdio>

using namespace rapidxml;

typedef xml_attribute<> XMLAttrib;

const char *XMLNode::own(const char *str) {
	return m_doc->allocate_string(str);
}

void XMLNode::addAttrib(const char *name, float value) {
	char str_value[64];
	snprintf(str_value, sizeof(str_value), "%f", value);
	addAttrib(name, own(str_value));
}

void XMLNode::addAttrib(const char *name, int value) {
	char str_value[32];
	sprintf(str_value, "%d", value);
	addAttrib(name, own(str_value));
}

void XMLNode::addAttrib(const char *name, const char *value) {
	m_ptr->append_attribute(m_doc->allocate_attribute(name, value));
}

void XMLNode::parsingError(const char *attrib_name) const {
	THROW("Error while parsing attribute value: %s in node: %s\n", attrib_name, name());
}

const char *XMLNode::attrib(const char *name) const {
	XMLAttrib *attrib = m_ptr->first_attribute(name);
	if(!attrib || !attrib->value())
		THROW("attribute not found: %s in node: %s\n", name, this->name());
	return attrib->value();
}

int XMLNode::intAttrib(const char *name) const {
	const char *str = attrib(name);
	int out;
	if(sscanf(str, "%d", &out) != 1)
		parsingError(name);
	return out;
}

float XMLNode::floatAttrib(const char *name) const {
	const char *str = attrib(name);
	float out;
	if(sscanf(str, "%f", &out) != 1)
		parsingError(name);
	return out;
}

const char *XMLNode::name() const {
	return m_ptr->name();
}

const char *XMLNode::value() const {
	return m_ptr->value();
}

XMLNode XMLNode::addChild(const char *name, const char *value) {
	xml_node<> *node = m_doc->allocate_node(node_element, name, value);
	m_ptr->append_node(node);
	return XMLNode(node, m_doc);
}

XMLNode XMLNode::sibling(const char *name) const {
	return XMLNode(m_ptr->next_sibling(name), m_doc);
}

XMLNode XMLNode::child(const char *name) const {
	return XMLNode(m_ptr->first_node(name), m_doc);
}



XMLDocument::XMLDocument() :m_ptr(new xml_document<>) { }
XMLDocument::XMLDocument(XMLDocument &&other) :m_ptr(std::move(other.m_ptr)) { }
XMLDocument::~XMLDocument() { }

void XMLDocument::operator=(XMLDocument &&other) {
	m_ptr = std::move(other.m_ptr);
}

const char *XMLDocument::own(const char *str) {
	return m_ptr->allocate_string(str);
}
	
XMLNode XMLDocument::addChild(const char *name, const char *value) const {
	xml_node<> *node = m_ptr->allocate_node(node_element, name, value);
	m_ptr->append_node(node);
	return XMLNode(node, m_ptr.get());
}

XMLNode XMLDocument::child(const char *name) const {
	return XMLNode(m_ptr->first_node(name), m_ptr.get());
}

void XMLDocument::load(const char *file_name) {
	DASSERT(file_name);
	Loader ldr(file_name);
	ldr & *this;

}

void XMLDocument::save(const char *file_name) {
	DASSERT(file_name);
	Saver svr(file_name);
	svr & *this;
}

void XMLDocument::serialize(Serializer &sr) {
	if(sr.isLoading()) {
		m_ptr->clear();

		char *xml_string = m_ptr->allocate_string(0, sr.size() + 1);
		sr.data(xml_string, sr.size());
		xml_string[sr.size()] = 0;
		
		try {
			m_ptr->parse<0>(xml_string); 
		} catch(const parse_error &ex) {
			THROW("rapidxml exception caught: %s at: %d", ex.what(), ex.where<char>() - xml_string);
		}
	}
	else {
		vector<char> buffer;
		print(std::back_inserter(buffer), *m_ptr);
		sr.data(&buffer[0], buffer.size());
	}
}
