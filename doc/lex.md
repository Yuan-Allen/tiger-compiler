- how I handle comments  
  When the scanner scanned a "/\*", the varible 'comment*level*' would be set to 1. Then we begin StartCondition\_\_::COMMENT, and each time we meet a "/\*", we increase comment*level* by one. Then if the scanner scanned a "\*/", the comment*level* would be decreased by one. We will check the value of comment_level\_ ,and if the value of comment_level\_ turned 0, we changed the StartCondition::COMMENT to StartCondition::INITIAL.

- how you handle strings  
  The procedure is similar with that of comments. Additionnal, we add much exclusive rules to handle escape sequences.

- error handling  
  Use adjust() and adjustStr() to update the char_pos\_ varible, which is helpful to log the error message. And when we find an error, we use errormsg\_->Error() to log.

- end-of-file handling  
  The flexc++ supports matching end-of-file with the token <\<EOF>>. And if we meet an end-of-file when we are handle comments or strings, we would like to consider it as an error.
