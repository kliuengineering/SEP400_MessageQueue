#include <errno.h>
#include <iostream>
#include <pthread.h>
#include <queue>
#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

// client header
#include "client.h"

// global variables
#define CLIENT_COUNT 3              // how many clients there are
#define MESSAGE_TYPE 4
bool is_running;
ssize_t rc;                         // for message queue
int message_id;                     // for message queue
pthread_mutex_t lock_x;             // for mutex
std::queue<Message> message_queue;
key_t key;                          // key for msg queue




/*************************** Utility Functions Begin ****************************/

// ISR setup and ISR function
static void ShutDownHandler(int sig)
{
    switch(sig)
    {
        case SIGINT:
            is_running = false;
        break;

        default:
            std::cout << "Unrecognized interrupt -> " << sig << " <- detected, abort...\n\n";
    }
}
void SignalHandlerSetup()
{
    struct sigaction action;
    action.sa_handler = ShutDownHandler;
    sigemptyset( &action.sa_mask );
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);
}


// message queue setup
void MessageQueueSetup()
{
    key = ftok("serverclient", 65);
    message_id = msgget( key, 0666 | IPC_CREAT );
    if( message_id < 0 )
    {
        std::cerr << "Error setting up message queue...\n";
        exit(EXIT_FAILURE);
    }
}


// receive thread
void *thread_recv(void *arg)
{
  std::cout << "Receive thread begins...\n\n";

  while(is_running)
  {
    Message message;

    // receive message from the message queue
    rc = msgrcv
                (
                    message_id, 
                    &message, 
                    sizeof(message), 
                    MESSAGE_TYPE, 
                    IPC_NOWAIT
                );

    if(rc < 0)
    {
        sleep(1);
    }
    else
    {
        pthread_mutex_lock(&lock_x);
            message_queue.push(message);
        pthread_mutex_unlock(&lock_x);
    }
  }

  pthread_exit(NULL);
}


// write thread
void *thread_write(void *arg)
{
    std::cout << "Write thread begins...\n\n";

    while(is_running)
    {
        if(message_queue.size())
        {
            pthread_mutex_lock(&lock_x);

                while(message_queue.size())
                {
                    Message tmp = message_queue.front();
                    message_queue.pop();

                    tmp.mtype = tmp.msgBuf.dest;
                    msgsnd( message_id, &tmp, sizeof(tmp), 0 );
                }
            
            pthread_mutex_unlock(&lock_x);
        }
    }

    // if is_running == false
    Message shut_down;
    sprintf( shut_down.msgBuf.buf, "Quit" );

    for( int i=0; i<CLIENT_COUNT; i++ )
    {
        shut_down.mtype = i+1;
        msgsnd(message_id, &shut_down, sizeof(shut_down), 0);
    }

    pthread_exit(NULL);
}


// thread starter
void ThreadStart(pthread_t &thread_id_recv, pthread_t &thread_id_write)
{
    rc = pthread_create(&thread_id_recv, NULL, thread_recv, NULL);
    if(rc != 0)
    {
        is_running = false;
        std::cerr << "Failed to start the receive thread...\n";
        exit(EXIT_FAILURE);
    }

    rc = pthread_create(&thread_id_write, NULL, thread_write, NULL);
    if(rc != 0)
    {
        is_running = false;
        std::cerr << "Failed to start the write thread...\n";
        exit(EXIT_FAILURE);
    }
}

/*************************** Utility Functions End ******************************/





int main(int argc, char *argv[])
{
    pthread_t tid_recv;
    pthread_t tid_write;

    is_running = true;
    pthread_mutex_init(&lock_x, NULL);
    SignalHandlerSetup();
    MessageQueueSetup();
    ThreadStart(tid_recv, tid_write);

    std::cout << "Server has started...\n";
    std::cout << "Press SIGINT to abort...\n";

    pthread_join(tid_recv, NULL);
    pthread_join(tid_write, NULL);

    std::cout << "Server is shutting down...\n";

    return EXIT_SUCCESS;
}