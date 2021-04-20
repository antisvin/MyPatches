#ifndef __LOCKABLE_VALUE_HPP__
#define __LOCKABLE_VALUE_HPP__

template <typename Value, typename T>
class LockableValue : public Value {
public:
    LockableValue(T delta = T(0), T value = T(0))
        : delta(delta)
        , value(value) {
    }
    void setLock(bool locked) {
        this->locked = locked;
    }
    bool isLocked() const {
        return locked;
    }
    void update(T newValue) {
        if (locked && abs(value.getValue() - newValue) <= delta)
            locked = false;
        if (!locked)
            value.update(newValue);
    }
    void reset(T newValue) {
        value = newValue;
    }
    T getValue() {
        return value.getValue();
    }
    LockableValue<Value, T>& operator=(const T& other) {
        if (!locked)
            value = other;
        return *this;
    }
    LockableValue<Value, T>& operator+=(const T& other) {
        if (!locked)
            value += other;
        return *this;
    }
    LockableValue<Value, T>& operator-=(const T& other) {
        if (!locked)
            value -= other;
        return *this;
    }
    LockableValue<Value, T>& operator*=(const T& other) {
        if (!locked)
            value *= other;
        return *this;
    }
    LockableValue<Value, T>& operator/=(const T& other) {
        if (!locked)
            value /= other;
        return *this;
    }
    Value& rawValue() {
        return value;
    }
    operator T() {
        return value.getValue();
    }

protected:
    Value value;
    T delta;
    bool locked = false;
};

#endif
