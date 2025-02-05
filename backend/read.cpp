/**
 * @file read.cpp
 * @brief RFID card reader and database interaction.
 *
 * This program continuously reads RFID tags using the MFRC522 reader and interacts with a MySQL database.
 * It checks if the UID of the scanned card exists in the database, updates the tap count and last scan time
 * for existing UIDs, or inserts new UIDs with an initial tap count and the current timestamp.
 *
 * @author Daniel Oh
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include "MFRC522.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/**
 * @brief Delays the execution for a specified amount of milliseconds.
 *
 * This function provides a cross-platform method to pause the program execution for a given duration.
 * It uses Sleep() on Windows and usleep() on Unix-based systems.
 *
 * @param ms The delay duration in milliseconds.
 */
void delay(int ms) {
#ifdef WIN32
    Sleep(ms);
#else
    usleep(ms * 1000); // usleep takes microseconds, so multiply milliseconds by 1000.
#endif
}

/**
 * @brief The main function that initializes the RFID reader and manages database interactions.
 *
 * Initializes the MFRC522 RFID reader and establishes a connection to the MySQL database. It continuously
 * checks for new RFID cards. Upon detecting a card, it queries the database for the card's UID. If the UID
 * exists, it increments the tap count and updates the last scan time. If the UID doesn't exist, it inserts
 * the UID into the database with an initial tap count of 1 and sets the current time as the last scan time.
 *
 * @returns int The exit code of the program. Returns 0 on normal termination.
 */
int main() {
    MFRC522 mfrc;
    mfrc.PCD_Init();

    sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
    std::unique_ptr<sql::Connection> conn(driver->connect("tcp://127.0.0.1:3307", "root", "root"));
    conn->setSchema("rfid_database");

    while (1) {
        if (!mfrc.PICC_IsNewCardPresent() || !mfrc.PICC_ReadCardSerial())
            continue;

        std::stringstream ss;
        for (byte i = 0; i < mfrc.uid.size; ++i) {
            if (i > 0)
                ss << "";
            ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(mfrc.uid.uidByte[i]);
        }

        std::string uidStr = ss.str();
        std::cout << "UID: " << uidStr << std::endl;

        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement("SELECT UID, tap_count, username FROM users WHERE UID = ?"));
            pstmt->setString(1, uidStr);
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

            if (res->next()) {
                std::string username = res->getString("username");
                int tapCount = res->getInt("tap_count") + 1;

                // Update tap count and last_scan_time in the database
                std::unique_ptr<sql::PreparedStatement> updatePstmt(conn->prepareStatement("UPDATE users SET tap_count = ?, last_scan_time = NOW() WHERE UID = ?"));
                updatePstmt->setInt(1, tapCount);
                updatePstmt->setString(2, uidStr);
                updatePstmt->executeUpdate();

                if (tapCount % 2 == 0) {
                    std::cout << "Goodbye " << username << std::endl;
                } else {
                    std::cout << "Hello " << username << std::endl;
                }
            } else {
                // Insert UID into the database with tap_count = 1 (first tap) and current timestamp
                pstmt.reset(conn->prepareStatement("INSERT INTO users (UID, tap_count, last_scan_time) VALUES (?, 1, NOW())"));
                pstmt->setString(1, uidStr);
                pstmt->executeUpdate();
                std::cout << "UID inserted into database." << std::endl;
            }
        } catch (sql::SQLException &e) {
            std::cerr << "SQL Exception: " << e.what() << std::endl;
        }

        delay(1000);
    }
    return 0;
}


