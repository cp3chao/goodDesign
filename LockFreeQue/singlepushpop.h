#pragma once


#include <atomic>
#include <memory>


//single push, single pop

template<typename T>
class SinglePopPush{

private:
    struct node
    {
        std::shared_ptr<T>data;
        node* next;
        node():next(nullptr){

        }
    };

    std::atomic<node*> head;
    std::atomic<node*> tail;

    node* pop_head(){

        node* const old_head = head.load();
        if(old_head == tail.load()) return nullptr;
        head.store(old_head->next);
        return old_head;
    }

public:
    SinglePopPush(): head(new node), tail(head.load()){

    }
    SinglePopPush(const SinglePopPush& other)=delete;
    SinglePopPush& operator=(const SinglePopPush& other) =delete;
    ~SinglePopPush(){
        
        while(node* const old_head= head.load()){
            head.store(old_head->next);
            delete old_head;
        }
    }

    std::shared_ptr<T> pop(){
        node* old_head = pop_head();
        if(!old_head) return shared_ptr<T>();

        std::shared_ptr<T> const res(old_head->data);
        delete old_head;
        return res;
    }
    
    void push(T new_value){
        std::shared_ptr<T> new_data(std::make_shared<T>(new_value));
        node* p = new node;
        node* const old_tail = tail.load();
        old_tail->data.swap(new_data);
        old_tail->next = p;
        tail.store(p);
    }

};