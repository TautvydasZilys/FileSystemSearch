#pragma once

#include "CriticalSection.h"
#include "Event.h"
#include "NonCopyable.h"

class EventQueue : NonCopyable
{
public:
    ~EventQueue()
    {
    }

    template <typename Callback>
    void InvokeAsync(Callback&& callback)
    {
        struct Invocation : IInvocation
        {
            Invocation(Callback&& callback) :
                m_Callback(std::move(callback))
            {
            }

            ~Invocation() override
            {
            }

            void Invoke() override
            {
                m_Callback();
            }

        private:
            Callback m_Callback;
        };

        auto invocation = new Invocation(std::move(callback));

        {
            CriticalSection::Lock lock(m_CallbacksLock);
            m_Callbacks.emplace_back(invocation);
        }

        m_CallbackAvailableEvent.Set();
    }

    void InvokeCallbacks()
    {
        Assert(m_PendingCallbacks.empty());

        {
            CriticalSection::Lock lock(m_CallbacksLock);

            for (auto& callback : m_Callbacks)
                m_PendingCallbacks.push_back(std::move(callback));

            m_Callbacks.clear();
        }

        for (auto& callback : m_PendingCallbacks)
            callback->Invoke();

        m_PendingCallbacks.clear();
    }

    HANDLE GetCallbackAvailableEvent() const
    {
        return m_CallbackAvailableEvent;
    }

private:
    struct IInvocation
    {
        virtual ~IInvocation() { }
        virtual void Invoke() = 0;
    };

private:
    CriticalSection m_CallbacksLock;
    Event<EventType::AutoReset> m_CallbackAvailableEvent;
    std::vector<std::unique_ptr<IInvocation>> m_Callbacks;
    std::vector<std::unique_ptr<IInvocation>> m_PendingCallbacks;
};