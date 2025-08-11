#pragma once

#include<atomic>
#include<memory>
#include<cassert>

template<typename T>
class lock_free_queue
{
private:
    struct node_counter
    {
        unsigned internal_count:30;
        unsigned external_counters: 2;
    };

    struct  node;

    struct counted_node_ptr
    {

        counted_node_ptr():external_count(0),ptr(nullptr){}
        int external_count;
        node* ptr;
    };
    
    struct node{
        std::atomic<T*> data;
        std::atomic<node_counter>count;
        std::atomic<counted_node_ptr>next;

        node(int external_count=2){
            node_counter new_count;
            new_count.internal_count = 0;
            new_count.external_counters = external_count;
            count.store(new_count);

            counted_node_ptr node_ptr;
            node_ptr.ptr = nullptr;
            node_ptr.external_count = 0;
            next.store(node_ptr);
        }


        void release_ref(){
            
            std::cout<<"call release ref"<<std::endl;
            node_counter  old_counter= count.load(std::memory_order_relaxed);
            node_counter new_counter;

            do{
                new_counter = old_counter;
                --new_counter.internal_count;
            }while(!count.compare_exchange_strong(
                old_counter, new_counter, std::memory_order_acquire, std::memory_order_relaxed
            ));


            if(!new_counter->internal_count && !new_counter->external_counters){
                delete this;
                std::cout<<"release_ref delete success"<<std::endl;
                destruct_count.fetch_add(1);
            }

        }
           
    };


    std::atomic<counted_node_ptr> head;
    std::atomic<counted_node_ptr> tail;


    void set_new_tail(counted_node_ptr& old_tail, counted_node_ptr const& new_tail){
        node* const current_tail_ptr = old_tail.ptr;

        while(!tail.compare_exchange_wead(old_tail,new_tail) && old_tail.ptr == current_tail_ptr);
        if(old_tail.ptr==current_tail_ptr)
            free_external_counter(old_tail);
        else
            current_tail_ptr->release_ref();
    }

    static void free_external_counter(counted_node_ptr& old_node_ptr){
        std::cout<<"call free_external_counter "<<std::endl;
        node* const ptr = old_node_ptr.ptr;
        int const count_increase = old_node_ptr.external_count-2;
        node_counter old_counter = ptr->count.load(std::memory_order_relaxed);
        node_counter new_counter;

        do{
            new_counter = old_counter;
            --new_counter.external_counters;
            new_counter.internal_count+= count_increase;

        }while(!ptr->count.compare_exchange_strong(
            old_counter, new_counter, std::memory_order_acquire, std::memory_order_relaxed
        ));

        if(!new_counter.internal_count && !new_counter.external_counters){
            destruct_count.fetch_add(1);
            std::cout<<"free_external_counter delete success"<<std::endl;
            delete ptr;
        }
    }

    static void increase_exteranl_count(std::atomic<counted_node_ptr>&counter, counted_node_ptr& old_counter){

        counted_node_ptr new_counter;
        do
        {
            new_counter = old_counter;
            ++new_counter.external_count;

        }while(!counter.compare_exchange_strong(old_counter, new_counter, std::memory_order_acquire, std::memory_order_relaxed));

        old_counter.external_count = new_counter.external_count;

    }
public:
    lock_free_queue(){
        counted_node_ptr new_next;
        new_next.ptr = new node();
        new_next.external_count = 1;
        tail.store(new_next);
        head.store(new_next);
        std::cout<<"new_next.ptr is "<< new_next.ptr<<std::endl;
    }

    ~lock_free_queue(){
        while(pop());
        auto head_counted_node = head.load();
        delete head_counted_node.ptr;
    }


    void push(T new_value){

        std::unique_ptr<T> new_data(new T(new_value));

        counted_node_ptr new_next;
        new_next.ptr = new node;
        new_next.external_count = 1;
        counted_node_ptr old_tail = tail.load();

        for(;;){
            increase_exteranl_count(tail, old_tail);
            T* old_data = nullptr;

            if(old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get()))
            {
                counted_node_ptr old_next;
                //counted_node_ptr now_next = old_tail.ptr->next.load();

                if(!old_tail.ptr->next.compare_exchange_strong(old_next,new_next)){
                    delete new_next.ptr;
                    new_next = old_next;
                }

                set_new_tail(old_tail, new_next);
                new_data.release();
                break;
          
            }
            else
            {
                counted_node_ptr old_next;

                if(old_tail.ptr->next.compare_exchange_strong(old_next, new_next)){
                    old_next = new_next;
                    new_next.ptr = new node;
                }
                 set_new_tail(old_tail, new_next);
            }   
        }
        construct_count++;
    }



    std::unique_ptr<T> pop(){

        counted_node_ptr old_head = head.load(std::memory_order_relaxed);

        for(;;){
            increase_exteranl_count(head, old_head);
            node* const ptr = old_head.ptr;

            if(ptr == tail.load().ptr){
                ptr->release_ref();
                return std::unique_ptr<T>();
            }


            counted_node_ptr next = ptr->next.load();
            if(head.compare_exchange_strong(old_head, next)){
                
                T* const res = ptr->data.exchange(nullptr);
                free_external_counter(old_head);
                return std::unique_ptr<T>(res);
            }

            ptr->release_ref();
        }

    }




    static std::atomic<int> destruct_count;
    static std::atomic<int> construct_count;
};



template<typename T>
std::atomic<int> lock_free_queue<T>::destruct_count=0;

template<typename T>
std::atomic<int> lock_free_queue<T>::construct_count =0;

