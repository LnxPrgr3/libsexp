# Welcome to libsexp!

The world's stuck on XML for hierarchal data storage, but this is madness. XML parsers are touchy beasts following a complex and strict standard and reading an obnoxious syntax, and they're blatant overkill for most of the work people give them.

Long before XML, there was another syntax for storing this kind of data: [S-expressions][^1].

I'll wait while you grab your parenthesis.

Got them? Great!

The syntax this library reads is really simple. Here's an example:

    (html

Compare this to XHTML:

    <!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"

This library produces events similar to what you'd get from a forward-only XML parser like libexpat, only without having to deal with XML.

[^1]: http://en.wikipedia.org/wiki/S-expression