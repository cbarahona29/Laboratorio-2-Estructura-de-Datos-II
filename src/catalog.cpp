#include "catalog.hpp"

#include "binary_io.hpp"
#include "catalog_codec.hpp"
#include "crc32.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace lab2 {

const char* to_string(ReadStatus status) {
    switch (status) {
    case ReadStatus::Ok: return "Ok";
    case ReadStatus::NotFound: return "NotFound";
    case ReadStatus::IndexKeyMismatch: return "IndexKeyMismatch";
    case ReadStatus::InvalidOffset: return "InvalidOffset";
    case ReadStatus::TruncatedHeader: return "TruncatedHeader";
    case ReadStatus::BadMagic: return "BadMagic";
    case ReadStatus::UnsupportedVersion: return "UnsupportedVersion";
    case ReadStatus::InvalidLength: return "InvalidLength";
    case ReadStatus::TruncatedPayload: return "TruncatedPayload";
    case ReadStatus::MissingChecksum: return "MissingChecksum";
    case ReadStatus::ChecksumMismatch: return "ChecksumMismatch";
    case ReadStatus::MalformedPayload: return "MalformedPayload";
    }
    return "UnknownReadStatus";
}

const char* to_string(BuildStatus status) {
    switch (status) {
    case BuildStatus::Ok: return "Ok";
    case BuildStatus::ReadError: return "ReadError";
    case BuildStatus::DuplicateKey: return "DuplicateKey";
    }
    return "UnknownBuildStatus";
}

const char* to_string(VerificationIssueType type) {
    switch (type) {
    case VerificationIssueType::UnsortedIndex: return "UnsortedIndex";
    case VerificationIssueType::DuplicateKey: return "DuplicateKey";
    case VerificationIssueType::DuplicateOffset: return "DuplicateOffset";
    case VerificationIssueType::RecordReadError: return "RecordReadError";
    case VerificationIssueType::KeyMismatch: return "KeyMismatch";
    }
    return "UnknownVerificationIssue";
}

ReadResult read_record_at(std::istream& input, std::uint64_t offset) {
    ReadResult result{ReadStatus::InvalidOffset, std::nullopt, offset, offset, {}};

    const auto size = stream_size(input);
    if (!size.has_value()) {
        result.detail = "No se pudo determinar el tamaño del archivo.";
        return result;
    }
    if (offset >= *size) {
        result.detail = "El offset está fuera del archivo.";
        return result;
    }
    if (!seek_absolute(input, offset)) {
        result.detail = "No se pudo posicionar el stream en el offset solicitado.";
        return result;
    }

    std::uint32_t magic = 0;
    std::uint16_t version = 0;
    std::uint32_t payload_length = 0;
    if (!read_u32_le(input, magic) ||
        !read_u16_le(input, version) ||
        !read_u32_le(input, payload_length)) {
        result.status = ReadStatus::TruncatedHeader;
        result.detail = "El header del registro está incompleto.";
        return result;
    }
    if (magic != RECORD_MAGIC) {
        result.status = ReadStatus::BadMagic;
        result.detail = "El registro no comienza con el magic MUS2.";
        return result;
    }
    if (version != RECORD_VERSION) {
        result.status = ReadStatus::UnsupportedVersion;
        result.detail = "La versión del registro no está soportada.";
        return result;
    }
    if (payload_length == 0 || payload_length > MAX_PAYLOAD_SIZE) {
        result.status = ReadStatus::InvalidLength;
        result.detail = "La longitud del payload es cero o excede 64 KiB.";
        return result;
    }

    std::vector<std::byte> payload(payload_length);
    if (!read_exact(input, payload)) {
        result.status = ReadStatus::TruncatedPayload;
        result.detail = "El payload está truncado.";
        return result;
    }

    std::uint32_t stored_checksum = 0;
    if (!read_u32_le(input, stored_checksum)) {
        result.status = ReadStatus::MissingChecksum;
        result.detail = "El CRC-32 almacenado está incompleto o ausente.";
        return result;
    }
    if (crc32(payload) != stored_checksum) {
        result.status = ReadStatus::ChecksumMismatch;
        result.detail = "El CRC-32 calculado no coincide con el almacenado.";
        return result;
    }

    PayloadDecodeResult decoded = decode_payload(payload);
    if (!decoded.record.has_value()) {
        result.status = ReadStatus::MalformedPayload;
        result.detail = std::move(decoded.detail);
        return result;
    }

    result.status = ReadStatus::Ok;
    result.record = std::move(decoded.record);
    result.next_offset = offset + RECORD_HEADER_SIZE + payload_length +
                         RECORD_CHECKSUM_SIZE;
    result.detail.clear();
    return result;
}

PrimaryBuildResult build_primary_index(std::istream& input) {
    PrimaryBuildResult result;
    const auto size = stream_size(input);
    if (!size.has_value()) {
        result.status = BuildStatus::ReadError;
        result.detail = "No se pudo determinar el tamaño del archivo.";
        return result;
    }

    std::uint64_t offset = 0;
    while (offset < *size) {
        ReadResult record_result = read_record_at(input, offset);
        if (!record_result.ok()) {
            result.status = BuildStatus::ReadError;
            result.error_offset = offset;
            result.detail = std::string(to_string(record_result.status)) +
                            ": " + record_result.detail;
            return result;
        }

        result.entries.push_back({record_result.record->label_id, offset});
        offset = record_result.next_offset;
    }

    std::sort(result.entries.begin(), result.entries.end(),
              [](const PrimaryEntry& left, const PrimaryEntry& right) {
                  return left.label_id < right.label_id;
              });
    for (std::size_t i = 1; i < result.entries.size(); ++i) {
        if (result.entries[i - 1].label_id == result.entries[i].label_id) {
            result.status = BuildStatus::DuplicateKey;
            result.error_key = result.entries[i].label_id;
            result.error_offset = result.entries[i].offset;
            result.detail = "La clave primaria aparece más de una vez.";
            return result;
        }
    }

    result.status = BuildStatus::Ok;
    result.detail.clear();
    return result;
}

std::optional<std::uint64_t> find_offset(
    std::span<const PrimaryEntry> index,
    std::string_view label_id) {
    std::size_t first = 0;
    std::size_t last = index.size();
    while (first < last) {
        const std::size_t middle = first + (last - first) / 2U;
        if (index[middle].label_id < label_id) {
            first = middle + 1U;
        } else if (label_id < index[middle].label_id) {
            last = middle;
        } else {
            return index[middle].offset;
        }
    }
    return std::nullopt;
}

ReadResult find_record(
    std::istream& input,
    std::span<const PrimaryEntry> index,
    std::string_view label_id) {
    const auto offset = find_offset(index, label_id);
    if (!offset.has_value()) {
        return {ReadStatus::NotFound, std::nullopt, 0, 0,
                "La clave no existe en el índice primario."};
    }
    ReadResult result = read_record_at(input, *offset);
    if (result.ok() && result.record->label_id != label_id) {
        result.status = ReadStatus::IndexKeyMismatch;
        result.record.reset();
        result.detail = "La clave del índice no coincide con la clave del registro.";
    }
    return result;
}

ComposerBuildResult build_composer_index(
    std::istream& input,
    std::span<const PrimaryEntry> primary) {
    struct ComposerPair {
        std::string composer;
        std::string label_id;
    };

    ComposerBuildResult result;
    std::vector<ComposerPair> pairs;
    pairs.reserve(primary.size());
    for (const PrimaryEntry& entry : primary) {
        ReadResult record_result = read_record_at(input, entry.offset);
        if (!record_result.ok()) {
            result.skipped.push_back(
                {entry.label_id, entry.offset, record_result.status});
            continue;
        }
        if (record_result.record->label_id != entry.label_id) {
            result.skipped.push_back(
                {entry.label_id, entry.offset, ReadStatus::IndexKeyMismatch});
            continue;
        }
        pairs.push_back(
            {record_result.record->composer, record_result.record->label_id});
    }

    std::sort(pairs.begin(), pairs.end(),
              [](const ComposerPair& left, const ComposerPair& right) {
                  if (left.composer != right.composer) {
                      return left.composer < right.composer;
                  }
                  return left.label_id < right.label_id;
              });
    for (const ComposerPair& pair : pairs) {
        if (result.entries.empty() ||
            result.entries.back().composer != pair.composer) {
            result.entries.push_back({pair.composer, {}});
        }
        auto& label_ids = result.entries.back().label_ids;
        if (label_ids.empty() || label_ids.back() != pair.label_id) {
            label_ids.push_back(pair.label_id);
        }
    }
    return result;
}

std::span<const std::string> find_by_composer(
    const ComposerIndex& index,
    std::string_view composer) {
    std::size_t first = 0;
    std::size_t last = index.size();
    while (first < last) {
        const std::size_t middle = first + (last - first) / 2U;
        if (index[middle].composer < composer) {
            first = middle + 1U;
        } else if (composer < index[middle].composer) {
            last = middle;
        } else {
            return index[middle].label_ids;
        }
    }
    return {};
}

VerificationReport verify_primary_index(
    std::istream& input,
    std::span<const PrimaryEntry> index) {
    VerificationReport report;
    report.entries_checked = index.size();

    for (std::size_t i = 1; i < index.size(); ++i) {
        if (index[i].label_id < index[i - 1].label_id) {
            report.issues.push_back({
                VerificationIssueType::UnsortedIndex, index[i].label_id,
                index[i].offset, ReadStatus::Ok,
                "La clave aparece antes que la entrada previa."});
        }
    }

    std::vector<PrimaryEntry> by_key(index.begin(), index.end());
    std::sort(by_key.begin(), by_key.end(),
              [](const PrimaryEntry& left, const PrimaryEntry& right) {
                  if (left.label_id != right.label_id) {
                      return left.label_id < right.label_id;
                  }
                  return left.offset < right.offset;
              });
    for (std::size_t i = 1; i < by_key.size(); ++i) {
        if (by_key[i].label_id == by_key[i - 1].label_id) {
            report.issues.push_back({
                VerificationIssueType::DuplicateKey, by_key[i].label_id,
                by_key[i].offset, ReadStatus::Ok,
                "La clave primaria está duplicada."});
        }
    }

    std::vector<PrimaryEntry> by_offset(index.begin(), index.end());
    std::sort(by_offset.begin(), by_offset.end(),
              [](const PrimaryEntry& left, const PrimaryEntry& right) {
                  if (left.offset != right.offset) {
                      return left.offset < right.offset;
                  }
                  return left.label_id < right.label_id;
              });
    for (std::size_t i = 1; i < by_offset.size(); ++i) {
        if (by_offset[i].offset == by_offset[i - 1].offset) {
            report.issues.push_back({
                VerificationIssueType::DuplicateOffset, by_offset[i].label_id,
                by_offset[i].offset, ReadStatus::Ok,
                "Más de una entrada apunta al mismo offset."});
        }
    }

    for (const PrimaryEntry& entry : index) {
        ReadResult record_result = read_record_at(input, entry.offset);
        if (!record_result.ok()) {
            report.issues.push_back({
                VerificationIssueType::RecordReadError, entry.label_id,
                entry.offset, record_result.status, record_result.detail});
        } else if (record_result.record->label_id != entry.label_id) {
            report.issues.push_back({
                VerificationIssueType::KeyMismatch, entry.label_id, entry.offset,
                ReadStatus::Ok,
                "La clave del índice no coincide con la clave del registro."});
        } else {
            ++report.readable_matching_entries;
        }
    }
    return report;
}

std::vector<std::string> intersect_sorted(
    std::span<const std::string> left,
    std::span<const std::string> right) {
    std::vector<std::string> result;
    std::size_t left_index = 0;
    std::size_t right_index = 0;
    while (left_index < left.size() && right_index < right.size()) {
        if (left[left_index] < right[right_index]) {
            ++left_index;
        } else if (right[right_index] < left[left_index]) {
            ++right_index;
        } else {
            if (result.empty() || result.back() != left[left_index]) {
                result.push_back(left[left_index]);
            }
            const std::string_view matched = left[left_index];
            while (left_index < left.size() && left[left_index] == matched) {
                ++left_index;
            }
            while (right_index < right.size() && right[right_index] == matched) {
                ++right_index;
            }
        }
    }
    return result;
}

} // namespace lab2
