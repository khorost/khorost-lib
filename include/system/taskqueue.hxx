#pragma once

#include <list>
#include "boost/thread/thread.hpp"
#include "boost/thread/future.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/make_shared.hpp"
#include "boost/foreach.hpp"
#include "boost/utility/result_of.hpp"

template <class TaskType>
class TaskQueue {
    typedef typename TaskType::result_type Result;
    typedef boost::shared_future<Result>  SharedFuture;
    typedef std::list<SharedFuture > Futures;
public:

    TaskQueue(std::size_t size = boost::thread::hardware_concurrency()) :
        work_(io_service_) {
        for (std::size_t i = 0; i < size; ++i) {
            // Create threads in pool
            threads_.create_thread(boost::bind(&TaskQueue::Run, this));
        }
    }

    ~TaskQueue() {
        io_service_.stop();
        threads_.join_all();
    }

    // Adds a task to the work queue.  Call blocks until there is an available thread.
    void QueueTask(TaskType task) {
        WaitForAvailableThread();
        typedef boost::packaged_task<Result> PackagedTask;
        boost::shared_ptr<PackagedTask> packaged_task_ptr = boost::make_shared<PackagedTask>(boost::bind(task));
        io_service_.post(boost::bind(&PackagedTask::operator(), packaged_task_ptr));
    }

    // Returns the number of tasks that are queued but not yet completed.
    int NumPendingTasks() {
        int num_pending = 0;
        BOOST_FOREACH(const SharedFuture& future, futures_) {
            if (!future.is_ready()) {
                ++num_pending;
            }
        }
        return num_pending;
    }

private:
    boost::asio::io_service io_service_;
    boost::asio::io_service::work work_;
    boost::thread_group threads_;
    Futures futures_;

    void Run() {
        io_service_.run();
    }

    void WaitForAvailableThread() {
        if (NumPendingTasks() >= threads_.size()) {
            boost::wait_for_any(futures_.begin(), futures_.end());
        }
    }
};
