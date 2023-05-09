# 42Exam_06

# Here explanation on how to build this code gradually in the exam

1. nest the provided code to you into two functions
you gonna have some code given to you, nest it into

  1.1 int openPortSocket(char const *port)
  1.2 int acceptConncetion/*or Client*/(const int socket)

2. make debug related helper functions and error exit function

  2.1 make argument handle errors print n exit

3. make select work with accepting connection only, print into terminal to learn that select's blocking behaviour works

4. make datastructure and methods of handling of datastructure, you need to be able to add filedescriptors to your structure. also ID

5. update select with a method that finds bigest FD, do add into data new accepted connections
 
  5.1 make a method that resets observation instructions for select
      methodName(socketfd, datastructure, infoming, outgoing)


6  implament - sendMessageToOthers(connection, message, current_sender_id)

  6.1. implament - informOfNewConnection(pointer to structure begining)

7. start implamenting client messageHandling(connectionstructure, incomingdfset, outgoingfdset)

  7.1 create funnction distributeMessages(connection, alldata)
  7.2 implament proper recv in it and print output into terminal.
  7.3 implament closing connection
  7.4 informOthers about the client disconected

8. create function that do actually distribute received messages-> called from distributeMessages like splitSend it helps to wrap message in a required format

  8.1 sendToOthersTheMessage.
  8.2 implament message format
  8.3 implament newline and no newlinehandling
