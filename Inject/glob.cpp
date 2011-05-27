#include "stdafx.h"
#include "glob.h"

// http://www.codeproject.com/KB/recipes/SimpleWildcardMatching.aspx

//////////////////////////////////////////////////////////////////////////
//    WildcardMatch
//        pszString    - Input string to match
//        pszMatch    - Match mask that may contain wildcards like ? and *
//    
//        A ? sign matches any character, except an empty string.
//        A * sign matches any string inclusive an empty string.
//        Characters are compared caseless.

bool WildcardMatch(const wchar_t *pszString, const wchar_t *pszMatch)
{
    // We have a special case where string is empty ("") and the mask is "*".
    // We need to handle this too. So we can't test on !*pszString here.
    // The loop breaks when the match string is exhausted.
    while (*pszMatch)
    {
        // Single wildcard character
        if (*pszMatch==TCHAR('?'))
        {
            // Matches any character except empty string
            if (!*pszString)
                return false;

            // OK next
            ++pszString;
            ++pszMatch;
        }
        else if (*pszMatch==TCHAR('*'))
        {
            // Need to do some tricks.

            // 1. The wildcard * is ignored. 
            //    So just an empty string matches. This is done by recursion.
            //      Because we eat one character from the match string, the
            //      recursion will stop.
            if (WildcardMatch(pszString,pszMatch+1))
                // we have a match and the * replaces no other character
                return true;

            // 2. Chance we eat the next character and try it again, with a
            //    wildcard * match. This is done by recursion. Because we eat
            //      one character from the string, the recursion will stop.
            if (*pszString && WildcardMatch(pszString+1,pszMatch))
                return true;

            // Nothing worked with this wildcard.
            return false;
        }
        else
        {
            // Standard compare of 2 chars. Note that *pszSring might be 0
            // here, but then we never get a match on *pszMask that has always
            // a value while inside this loop.
            if (::CharUpper(MAKEINTRESOURCE(MAKELONG(*pszString++,0)))
		!=::CharUpper(MAKEINTRESOURCE(MAKELONG(*pszMatch++,0))))
                return false;
        }
    }

    // Have a match? Only if both are at the end...
    return !*pszString && !*pszMatch;
}