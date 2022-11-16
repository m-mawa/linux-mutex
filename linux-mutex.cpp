#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>

const char SHARED_FILE_NAME[] = "_t_timer_shared_file";

// Размер содержимого файла (размер мьютекса + размер атрибутов мьютекса + счетчик)
const size_t MEMORY_SIZE = sizeof(pthread_mutex_t) + sizeof(pthread_mutexattr_t) + sizeof(unsigned long);


class TimerProcessDescriptor {
public:
    explicit TimerProcessDescriptor(const char *shared_file_name, const char* process_name) : name(process_name) {

        bool need_to_init = false;
        auto file_descriptor = get_file_descriptor(shared_file_name, need_to_init);

        // Первый адрес - мьютекс, второй атрибуты, третий счетчики
        void *shared_memory_ptr = mmap(
                nullptr,
                MEMORY_SIZE,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                file_descriptor,
                0
        );

        this->mutex_ptr = (pthread_mutex_t *)(shared_memory_ptr);

        this->mutex_attr_ptr = (pthread_mutexattr_t*)( ((char *)mutex_ptr) + sizeof(pthread_mutex_t) );

        this->timer_ptr = (unsigned long *) ( ((char *)mutex_attr_ptr) + sizeof(pthread_mutexattr_t) );

        if (need_to_init)
            this->init();
    }

    void step(){
        pthread_mutex_lock(mutex_ptr);
        for (size_t i = 0; i < 5; i++) {
            sleep(1);
            std::cout << "Таймер: " << name << ". Секунд прошло: " << *timer_ptr << std::endl;
            *timer_ptr += 1;
        }
        pthread_mutex_unlock(mutex_ptr);
    }
private:
    pthread_mutex_t* mutex_ptr {nullptr};
    pthread_mutexattr_t* mutex_attr_ptr {nullptr};
    unsigned long* timer_ptr {nullptr};
    std::string name;

    void init() {
        pthread_mutexattr_init(mutex_attr_ptr);
        // Оптимизация использования мьютекса для нескольких процессов
        pthread_mutexattr_setpshared(mutex_attr_ptr, PTHREAD_PROCESS_SHARED);
        // Установка порядка передачи мьютекса таким образом, чтобы он достался всем
        pthread_mutexattr_setprotocol(mutex_attr_ptr, PTHREAD_PRIO_INHERIT);
        pthread_mutex_init(mutex_ptr, mutex_attr_ptr);
        *timer_ptr = 1;
    }

    /* Возвращает дескриптор файла общего для нескольких потоков */
    static int get_file_descriptor(const char* filename, bool &need_to_init_mutex) {
        int file = open(filename, O_RDWR | O_CREAT | O_EXCL, 0666);
        if (file != -1) {
            ftruncate(file, MEMORY_SIZE);
            need_to_init_mutex = true;
        } else {
            file = open(filename, O_RDWR, 0666);
        }

        if (file == -1)
            throw std::runtime_error("Не удалось открыть файл!");

        return file;
    }
};


int main(int argc, char **argv) {

    char *name = argv[1];

    TimerProcessDescriptor process (SHARED_FILE_NAME, name);

    while (true) {
        process.step();
    }
}
