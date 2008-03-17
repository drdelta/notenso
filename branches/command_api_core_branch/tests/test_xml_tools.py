"""
    Test cases for enso.utils.xml_tools.
"""

# ----------------------------------------------------------------------------
# Imports
# ----------------------------------------------------------------------------

import unittest
import xml.dom.minidom
import xml.sax

from enso.utils import xml_tools


# ----------------------------------------------------------------------------
# Unit Tests
# ----------------------------------------------------------------------------

class InnerTextTests( unittest.TestCase ):

        
    TESTS = []
    TESTS.append( """<xml><![CDATA[abcdefg]]>hij<A>kl<B>mn</B>opqrs<C>t</C><![CDATA[uvw]]>x</A>yz</xml>""" )
    TESTS.append( """<xml><![CDATA[abcdefghijklmnopqrstuvwxzy]]></xml>""" )
    TESTS.append( """<xml>a<![CDATA[b]]>cdefghijklmnopqrstuvwxyz</xml>""" )

    def testInvalidControlCharactersRaiseExceptions( self ):
        handler = xml.sax.handler.ContentHandler()
        for char in xml_tools.INVALID_CONTROL_CHARACTERS:
            xmlStr = "<test>%s</test>" % chr( char )
            self.assertRaises(
                xml.sax.SAXParseException,
                xml.sax.parseString,
                xmlStr,
                handler
                )

    def testRemoveInvalidControlCharactersWorksOnNonUnicodeStrings( self ):
        handler = xml.sax.handler.ContentHandler()
        for char in xml_tools.INVALID_CONTROL_CHARACTERS:
            xmlStr = "<test>%s</test>" % chr( char )
            xmlStr = xml_tools.removeInvalidControlCharacters( xmlStr )
            self.assertEquals( xmlStr, "<test></test>" )
            xml.sax.parseString( xmlStr, handler )

    def testRemoveInvalidControlCharactersWorksOnUnicodeStrings( self ):
        handler = xml.sax.handler.ContentHandler()
        for char in xml_tools.INVALID_CONTROL_CHARACTERS:
            xmlStr = u"<test>%s\u2026</test>" % chr( char )
            xmlStr = xml_tools.removeInvalidControlCharacters( xmlStr )
            self.assertEquals( xmlStr, u"<test>\u2026</test>" )
            xmlStr = xmlStr.encode( "ascii", "xmlcharrefreplace" )
            xml.sax.parseString( xmlStr, handler )

    def testEscapeXmlRemovesInvalidControlCharacters( self ):
        for char in xml_tools.INVALID_CONTROL_CHARACTERS:
            xmlStr = u"%s\u2026" % chr( char )
            self.assertEquals( xml_tools.escapeXml(xmlStr),
                               u"\u2026" )
            xmlStr = "%shi" % chr( char )
            self.assertEquals( xml_tools.escapeXml(xmlStr),
                               "hi" )
            return xmlStr

    def testWierdCases( self ):
        for case in self.TESTS:
            xmlData = case
            innerText = "abcdefghijklmnopqrstuvwxyz"

        document = xml.dom.minidom.parseString( xmlData )

        results = xml_tools.getInnerText( document )
        self.failUnlessEqual( results, innerText )
        
    def testManyTags( self ):

        cases = range( ord("a"), ord("z") )
        cases = [ chr( c ) for c in cases ]

        xmlData = "<xml>\n"
        for c in cases:
            xmlData += ( "  <%s>%s</%s>\n" % ( c, c.upper(), c ) )
        xmlData += "</xml>"

        document = xml.dom.minidom.parseString( xmlData )

        for c in cases:
            nodes = document.getElementsByTagName( c )
            # There should be only one dom node for each tag name.
            self.failUnlessEqual( len(nodes), 1 )
            # The inner xml should be exactly the tag name, uppercased.
            results = xml_tools.getInnerText( nodes[0] )
            self.failUnlessEqual( results, c.upper() )

    def testMuchRecursion( self ):
        cases = range( ord("a"), ord("z") )
        cases = [ chr( c ) for c in cases ]

        xmlData = "<xml>%s</xml>"
        innerText = ""
        for c in cases[:-1]:
            xmlData %= ( "<%s>%s</%s>" % ( c, c.upper()+"%s", c ) )
            innerText += c.upper()
        c = cases[-1]
        xmlData %= ( "<%s>%s</%s>" % ( c, c.upper(), c ) )
        innerText += c.upper()

        document = xml.dom.minidom.parseString( xmlData )

        # There should be only one dom node for the "xml" tag name.
        nodes = document.getElementsByTagName( "xml" )
        self.failUnlessEqual( len(nodes), 1 )
        
        # The inner xml should be exactly the tag name, uppercased.
        results = xml_tools.getInnerText( nodes[0] )
        self.failUnlessEqual( results, innerText )
            

# ----------------------------------------------------------------------------
# Script
# ----------------------------------------------------------------------------

if __name__ == "__main__":
    unittest.main()
