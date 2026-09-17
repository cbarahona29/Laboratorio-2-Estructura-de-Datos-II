#include "doctest/doctest.h"

#include "catalog.hpp"
#include "catalog_codec.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct StudentFixture {
    std::string bytes;
    std::vector<std::uint64_t> offsets;
};

StudentFixture make_student_fixture(const std::vector<lab2::Record>& records) {
    std::ostringstream output(std::ios::binary | std::ios::out);
    StudentFixture fixture;
    for (const auto& record : records) {
        std::uint64_t offset = 0;
        std::string error;
        REQUIRE(lab2::write_record(output, record, &offset, error));
        fixture.offsets.push_back(offset);
    }
    fixture.bytes = output.str();
    return fixture;
}

std::stringstream open_student_fixture(const std::string& bytes) {
    return std::stringstream(bytes, std::ios::binary | std::ios::in | std::ios::out);
}

} // namespace

TEST_CASE("A01 - un archivo vacío produce un índice primario vacío") {
    auto input = open_student_fixture("");
    const auto result = lab2::build_primary_index(input);

    CHECK(result.status == lab2::BuildStatus::Ok);
    CHECK(result.entries.empty());
}

TEST_CASE("A02 - read_record_at distingue header truncado") {
    const std::string bytes{"MUS2", 4};
    auto input = open_student_fixture(bytes);

    const auto result = lab2::read_record_at(input, 0);
    CHECK(result.status == lab2::ReadStatus::TruncatedHeader);
    CHECK_FALSE(result.record.has_value());
}

TEST_CASE("A03 - el índice secundario omite una clave que no coincide") {
    const auto fixture = make_student_fixture({
        {"REAL-1", "MOZART", "REQUIEM"}
    });
    const std::vector<lab2::PrimaryEntry> primary{
        {"WRONG-1", fixture.offsets[0]}
    };
    auto input = open_student_fixture(fixture.bytes);

    const auto result = lab2::build_composer_index(input, primary);
    CHECK(result.entries.empty());
    REQUIRE(result.skipped.size() == 1);
    CHECK(result.skipped[0].status == lab2::ReadStatus::IndexKeyMismatch);
}

TEST_CASE("A04 - la auditoría encuentra duplicados aun con índice desordenado") {
    const auto fixture = make_student_fixture({
        {"A", "COMPOSER", "TITLE A"},
        {"B", "COMPOSER", "TITLE B"}
    });
    const std::vector<lab2::PrimaryEntry> primary{
        {"B", fixture.offsets[1]},
        {"A", fixture.offsets[0]},
        {"B", fixture.offsets[1]}
    };
    auto input = open_student_fixture(fixture.bytes);
    const auto report = lab2::verify_primary_index(input, primary);

    bool has_unsorted = false;
    bool has_duplicate_key = false;
    bool has_duplicate_offset = false;
    for (const auto& issue : report.issues) {
        has_unsorted |= issue.type == lab2::VerificationIssueType::UnsortedIndex;
        has_duplicate_key |= issue.type == lab2::VerificationIssueType::DuplicateKey;
        has_duplicate_offset |= issue.type == lab2::VerificationIssueType::DuplicateOffset;
    }
    CHECK(has_unsorted);
    CHECK(has_duplicate_key);
    CHECK(has_duplicate_offset);
}

TEST_CASE("A05 - la intersección es ordenada y elimina duplicados") {
    const std::vector<std::string> left{"A", "A", "C", "E", "E"};
    const std::vector<std::string> right{"A", "B", "E", "E", "F"};

    CHECK(lab2::intersect_sorted(left, right) ==
          std::vector<std::string>{"A", "E"});
}
