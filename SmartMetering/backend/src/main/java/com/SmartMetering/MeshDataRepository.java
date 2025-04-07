package com.SmartMetering;
import java.util.List;
import org.springframework.data.jpa.repository.JpaRepository;

public interface MeshDataRepository extends JpaRepository<MeshData, Long> {
    List<MeshData> findAll();
    MeshData save(MeshData data);
}
