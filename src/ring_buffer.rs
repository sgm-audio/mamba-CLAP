use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::mpsc;
use std::thread;
use std::time::Duration;

/// A simple ring buffer for inter-thread communication
/// Note: This implementation uses Mutex for simplicity in testing
/// A production version would use proper lock-free techniques
#[derive(Debug)]
pub struct RingBuffer<T: Clone> {
    buffer: Vec<Option<T>>,
    capacity: usize,
    head: AtomicUsize, // Index where producer writes
    tail: AtomicUsize, // Index where consumer reads
    closed: AtomicBool,
}

impl<T: Clone> RingBuffer<T> {
    /// Create a new ring buffer with the given capacity
    pub fn new(capacity: usize) -> Self {
        assert!(capacity > 0, "Capacity must be greater than 0");
        Self {
            buffer: vec![None; capacity],
            capacity,
            head: AtomicUsize::new(0),
            tail: AtomicUsize::new(0),
            closed: AtomicBool::new(false),
        }
    }

    /// Push an item into the buffer (producer side)
    /// Returns Ok(()) if successful, Err(item) if buffer is full
    pub fn push(&mut self, item: T) -> Result<(), T> {
        if self.closed.load(Ordering::Acquire) {
            return Err(item);
        }

        let mut head = self.head.load(Ordering::Relaxed);
        loop {
            let next_head = (head + 1) % self.capacity;

            // Check if buffer is full
            if next_head == self.tail.load(Ordering::Acquire) {
                return Err(item);
            }

            // Try to claim the slot
            if self.head
                .compare_exchange_weak(head, next_head, Ordering::Release, Ordering::Relaxed)
                .is_ok()
            {
                // Successfully claimed the slot
                self.buffer[head] = Some(item);
                return Ok(());
            }

            // Failed to claim, retry with updated head
            head = self.head.load(Ordering::Relaxed);
        }
    }

    /// Pop an item from the buffer (consumer side)
    /// Returns Some(item) if available, None if buffer is empty or closed
    pub fn pop(&mut self) -> Option<T> {
        if self.tail.load(Ordering::Acquire) == self.head.load(Ordering::Acquire)
            && self.closed.load(Ordering::Acquire) {
            return None; // Buffer is empty and closed
        }

        let mut tail = self.tail.load(Ordering::Relaxed);
        loop {
            // Check if buffer is empty
            if tail == self.head.load(Ordering::Acquire) {
                // Check if closed
                if self.closed.load(Ordering::Acquire) {
                    return None;
                }
                // Buffer is empty but not closed - keep trying
                return None;
            }

            let next_tail = (tail + 1) % self.capacity;

            // Try to claim the slot
            if self.tail
                .compare_exchange_weak(tail, next_tail, Ordering::Release, Ordering::Relaxed)
                .is_ok()
            {
                // Successfully claimed the slot
                return self.buffer[tail].take();
            }

            // Failed to claim, retry with updated tail
            tail = self.tail.load(Ordering::Relaxed);
        }
    }

    /// Try to pop an item without blocking
    /// Returns Some(item) if available, None if buffer is empty
    pub fn try_pop(&mut self) -> Option<T> {
        if self.tail.load(Ordering::Acquire) == self.head.load(Ordering::Acquire) {
            return None; // Buffer is empty
        }

        let tail = self.tail.load(Ordering::Relaxed);
        let next_tail = (tail + 1) % self.capacity;

        // Check if another thread is trying to pop the same item
        if self.tail
            .compare_exchange(tail, next_tail, Ordering::Release, Ordering::Relaxed)
            .is_ok()
        {
            // Successfully claimed the slot
            return self.buffer[tail].take();
        }

        None
    }

    /// Close the buffer (no more items will be pushed)
    pub fn close(&mut self) {
        self.closed.store(true, Ordering::Release);
    }

    /// Check if the buffer is closed
    pub fn is_closed(&self) -> bool {
        self.closed.load(Ordering::Acquire)
    }

    /// Get the current number of items in the buffer
    pub fn len(&self) -> usize {
        let head = self.head.load(Ordering::Acquire);
        let tail = self.tail.load(Ordering::Acquire);

        if head >= tail {
            head - tail
        } else {
            self.capacity - tail + head
        }
    }

    /// Check if the buffer is empty
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Check if the buffer is full
    pub fn is_full(&self) -> bool {
        let head = self.head.load(Ordering::Acquire);
        let tail = self.tail.load(Ordering::Acquire);
        let next_head = (head + 1) % self.capacity;
        next_head == tail
    }
}

impl<T: Clone> Drop for RingBuffer<T> {
    fn drop(&mut self) {
        // Clean up any remaining items
        self.close();
        while let Some(_) = self.pop() {
            // Drop items
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::{Arc, Mutex};
    use std::sync::mpsc;
    use std::thread;
    use std::time::Duration;

    #[test]
    fn test_ring_buffer_basic() {
        let mut rb = RingBuffer::<i32>::new(4);

        // Test push and pop
        assert_eq!(rb.push(1), Ok(()));
        assert_eq!(rb.push(2), Ok(()));
        assert_eq!(rb.pop(), Some(1));
        assert_eq!(rb.pop(), Some(2));
        assert_eq!(rb.pop(), None);
    }

    #[test]
    fn test_ring_buffer_full() {
        let mut rb = RingBuffer::<i32>::new(2);

        // Fill the buffer
        assert_eq!(rb.push(1), Ok(()));
        assert_eq!(rb.push(2), Ok(()));

        // Buffer should be full
        assert!(rb.is_full());

        // Next push should fail
        assert_eq!(rb.push(3), Err(3));

        // After popping, should be able to push again
        assert_eq!(rb.pop(), Some(1));
        assert_eq!(rb.push(3), Ok(()));
    }

    #[test]
    fn test_ring_buffer_try_pop() {
        let mut rb = RingBuffer::<i32>::new(2);

        // Should be empty initially
        assert_eq!(rb.try_pop(), None);

        // Add an item
        rb.push(42).unwrap();

        // Should be able to try pop it
        assert_eq!(rb.try_pop(), Some(42));
        assert_eq!(rb.try_pop(), None);
    }

    #[test]
    fn test_ring_buffer_close() {
        let mut rb = RingBuffer::<i32>::new(2);

        // Push some items
        rb.push(1).unwrap();
        rb.push(2).unwrap();

        // Close the buffer
        rb.close();
        assert!(rb.is_closed());

        // Should still be able to pop existing items
        assert_eq!(rb.pop(), Some(1));
        assert_eq!(rb.pop(), Some(2));

        // Should return None when empty and closed
        assert_eq!(rb.pop(), None);

        // Push should fail when closed
        assert_eq!(rb.push(3), Err(3));
    }

    #[test]
    fn test_ring_buffer_producer_consumer() {
        let rb = Arc::new(Mutex::new(RingBuffer::<i32>::new(10)));
        let rb_consumer = Arc::clone(&rb);

        let (tx1, rx1) = mpsc::channel();
        let (tx2, rx2) = mpsc::channel();

        // Producer thread
        let producer = thread::spawn(move || {
            let mut rb_lock = rb.lock().unwrap();
            for i in 0..20 {
                // Push with retry if buffer is full
                while let Err(_item) = rb_lock.push(i) {
                    drop(rb_lock);
                    thread::sleep(Duration::from_millis(1));
                    rb_lock = rb.lock().unwrap();
                }
            }
            rb_lock.close();
            drop(rb_lock);
            tx1.send(()).unwrap();
        });

        // Consumer thread
        let consumer = thread::spawn(move || {
            let mut received = Vec::new();
            loop {
                let mut rb_lock = rb_consumer.lock().unwrap();
                if let Some(item) = rb_lock.pop() {
                    received.push(item);
                    drop(rb_lock);
                } else if rb_lock.is_closed() && rb_lock.is_empty() {
                    drop(rb_lock);
                    break;
                } else {
                    drop(rb_lock);
                    thread::sleep(Duration::from_millis(1));
                }
            }
            // Send the received data back through the channel
            let _ = tx2.send(received);
        });

        // Wait for producer to finish
        producer.join().unwrap();
        rx1.recv().unwrap(); // Wait for producer signal

        // Wait for consumer to finish and get results
        let received = rx2.recv().unwrap();

        // Check that we received all 20 items in order
        assert_eq!(received.len(), 20);
        for i in 0..20 {
            assert_eq!(received[i], i as i32);
        }
    }
}
